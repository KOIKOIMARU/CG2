#include "engine/scene/TitleScene.h"
#include "app/CombatHud.h"
#include "app/MenuUi.h"
#include "engine/3d/Camera.h"
#include "engine/3d/ModelManager.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/3d/ModelCommon.h"
#include "engine/audio/SoundManager.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/SrvManager.h"
#include "engine/io/Input.h"
#include "engine/scene/GameScene.h"
#include "engine/scene/SceneManager.h"
#include <dinput.h>
#include <imgui.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <initializer_list>
#include <stdexcept>

namespace {
constexpr const char* kTitle = "AZRAID";
constexpr const char* kTitleJapanese = "アズレイド";
constexpr const char* kShip = "free_models/player_candidates/Omen.gltf";
constexpr const char* kSky = "resources/skybox/kloofendal_48d_partly_cloudy_puresky_4k_cube.dds";
constexpr float kShipScale = 2.25f;
constexpr float kDepartureDuration = 2.15f;


float Smooth(float value)
{
    const float t = std::clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

Math::Vector3 RotateVector(const Math::Vector3& value, const Math::Matrix4x4& matrix)
{
    return {
        value.x * matrix.m[0][0] + value.y * matrix.m[1][0] + value.z * matrix.m[2][0],
        value.x * matrix.m[0][1] + value.y * matrix.m[1][1] + value.z * matrix.m[2][1],
        value.x * matrix.m[0][2] + value.y * matrix.m[1][2] + value.z * matrix.m[2][2] };
}

ModelData CreateWordmarkMesh(char glyph)
{
    // 文字を画像にせず、輪郭から厚みのあるメッシュを組み立てる。
    ModelData mesh;
    mesh.material.textureFilePath = "resources/human/white.png";
    mesh.vertices.reserve(1200);
    const auto triangle = [&](Math::Vector3 a, Math::Vector3 b, Math::Vector3 c, Math::Vector3 normal) {
        for (const auto p : { a, b, c }) {
            mesh.vertices.push_back({ {p.x, p.y, p.z, 1}, {0.5f, 0.5f}, normal, {}, {} });
        }
    };
    float pen = 0.0f;
    const auto polygon = [&](std::initializer_list<ImVec2> shape) {
        std::array<Math::Vector3, 8> points{};
        size_t count = 0;
        for (const ImVec2 point : shape) {
            points[count++] = { (pen + point.x + (90.0f - point.y) * 0.17f) * 0.028f,
                (90.0f - point.y) * 0.028f, 0.0f };
        }
        for (size_t index = 1; index + 1 < count; ++index) {
            triangle(points[0], points[index], points[index + 1], {0,0,-1});
            auto backA = points[0], backB = points[index], backC = points[index + 1];
            backA.z = backB.z = backC.z = 0.18f;
            triangle(backA, backC, backB, {0,0,1});
        }
        for (size_t index = 0; index < count; ++index) {
            const auto frontA = points[index], frontB = points[(index + 1) % count];
            auto backA = frontA, backB = frontB;
            backA.z = backB.z = 0.18f;
            const Math::Vector3 normal = Math::Normalize({ frontA.y - frontB.y, frontB.x - frontA.x, 0 });
            triangle(frontA, backA, backB, normal);
            triangle(frontA, backB, frontB, normal);
        }
    };
    const char text[] = { glyph, '\0' };
    for (const char* letter = text; *letter; ++letter) {
        switch (*letter) {
        case 'A':
            polygon({ {0,90}, {29,0}, {44,0}, {17,90} });
            polygon({ {31,0}, {46,0}, {70,90}, {53,90} });
            polygon({ {19,55}, {51,55}, {55,69}, {15,69} });
            pen += 76.0f;
            break;
        case 'Z':
            polygon({ {0,0}, {67,0}, {67,15}, {0,15} });
            polygon({ {47,14}, {67,14}, {20,76}, {0,76} });
            polygon({ {0,75}, {67,75}, {67,90}, {0,90} });
            pen += 75.0f;
            break;
        case 'R':
            polygon({ {0,0}, {15,0}, {15,90}, {0,90} });
            polygon({ {14,0}, {54,0}, {68,15}, {14,15} });
            polygon({ {54,14}, {68,14}, {68,40}, {54,40} });
            polygon({ {14,39}, {68,39}, {54,54}, {14,54} });
            polygon({ {30,52}, {48,52}, {72,90}, {53,90} });
            pen += 79.0f;
            break;
        case 'I':
            polygon({ {0,0}, {16,0}, {16,90}, {0,90} });
            pen += 26.0f;
            break;
        case 'D':
            polygon({ {0,0}, {15,0}, {15,90}, {0,90} });
            polygon({ {14,0}, {52,0}, {70,17}, {54,18}, {14,15} });
            polygon({ {54,16}, {70,17}, {70,73}, {54,75} });
            polygon({ {14,75}, {70,73}, {52,90}, {14,90} });
            pen += 76.0f;
            break;
        }
    }
    return mesh;
}


}

// 雲は低解像度で描いて線形拡大。自機とロゴは通常解像度で描く。
class TitleAtmosphere {
public:
    ~TitleAtmosphere()
    {
        if (srvManager_ && textureSrv_ != UINT32_MAX) { srvManager_->Free(textureSrv_); }
    }

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
    {
        dxCommon_ = dxCommon;
        srvManager_ = srvManager;
        D3D12_ROOT_PARAMETER parameter{};
        parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        parameter.Constants.Num32BitValues = 16;
        D3D12_ROOT_SIGNATURE_DESC rootDesc{};
        rootDesc.NumParameters = 1;
        rootDesc.pParameters = &parameter;
        Microsoft::WRL::ComPtr<ID3DBlob> signature, error;
        if (FAILED(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1,
            &signature, &error))) { throw std::runtime_error("Title atmosphere root signature"); }
        auto* device = dxCommon_->GetDevice();
        if (FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(),
            signature->GetBufferSize(), IID_PPV_ARGS(&root_)))) {
            throw std::runtime_error("Title atmosphere root creation");
        }
        const auto vs = dxCommon_->CompileShader(L"shaders/Fullscreen.VS.hlsl", L"vs_6_0");
        const auto ps = dxCommon_->CompileShader(L"shaders/TitleSky.PS.hlsl", L"ps_6_0");
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
        desc.pRootSignature = root_.Get();
        desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
        desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
        desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        desc.RasterizerState.DepthClipEnable = TRUE;
        desc.DepthStencilState.DepthEnable = FALSE;
        desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        desc.NumRenderTargets = 1;
        desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        desc.SampleDesc.Count = 1;
        desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
        if (FAILED(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline_)))) {
            throw std::runtime_error("Title atmosphere pipeline");
        }

        texture_ = dxCommon_->CreateRenderTextureResource(device, kWidth, kHeight,
            DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, {0,0,0,1});
        rtv_ = dxCommon_->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, false);
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
        rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        device->CreateRenderTargetView(texture_.Get(), &rtvDesc, rtv_->GetCPUDescriptorHandleForHeapStart());
        textureSrv_ = srvManager_->Allocate();
        srvManager_->CreateSRVforTexture2D(textureSrv_, texture_.Get(), rtvDesc.Format, 1);

        D3D12_DESCRIPTOR_RANGE range{};
        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        range.NumDescriptors = 1;
        range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameter.DescriptorTable.NumDescriptorRanges = 1;
        parameter.DescriptorTable.pDescriptorRanges = &range;
        D3D12_STATIC_SAMPLER_DESC sampler{};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootDesc.NumStaticSamplers = 1;
        rootDesc.pStaticSamplers = &sampler;
        signature.Reset(); error.Reset();
        if (FAILED(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)) ||
            FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                IID_PPV_ARGS(&compositeRoot_)))) { throw std::runtime_error("Title atmosphere composite root"); }
        const auto compositePs = dxCommon_->CompileShader(L"shaders/TitleSkyComposite.PS.hlsl", L"ps_6_0");
        desc.pRootSignature = compositeRoot_.Get();
        desc.PS = { compositePs->GetBufferPointer(), compositePs->GetBufferSize() };
        desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        if (FAILED(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&compositePipeline_)))) {
            throw std::runtime_error("Title atmosphere composite pipeline");
        }
    }

    void Draw(const Camera& camera, float elapsed, float departure)
    {
        const auto& world = camera.GetWorldMatrix();
        const float tangent = std::tan((0.62f + 0.065f * departure) * 0.5f);
        const float travel = elapsed * 22.0f + departure * 460.0f;
        const std::array<float, 16> parameters{
            world.m[0][0], world.m[0][1], world.m[0][2], tangent * camera.GetAspectRatio(),
            world.m[1][0], world.m[1][1], world.m[1][2], tangent,
            world.m[2][0], world.m[2][1], world.m[2][2], 0,
            -80.0f * std::sin(travel * 0.0013f), 340.0f, travel, 0 };
        auto* command = dxCommon_->GetCommandList();
        Transition(D3D12_RESOURCE_STATE_RENDER_TARGET);
        const auto cloudRtv = rtv_->GetCPUDescriptorHandleForHeapStart();
        command->OMSetRenderTargets(1, &cloudRtv, FALSE, nullptr);
        const D3D12_VIEWPORT cloudViewport{0,0,static_cast<float>(kWidth),static_cast<float>(kHeight),0,1};
        const D3D12_RECT cloudScissor{0,0,kWidth,kHeight};
        command->RSSetViewports(1, &cloudViewport);
        command->RSSetScissorRects(1, &cloudScissor);
        command->SetGraphicsRootSignature(root_.Get());
        command->SetPipelineState(pipeline_.Get());
        command->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command->SetGraphicsRoot32BitConstants(0, static_cast<UINT>(parameters.size()), parameters.data(), 0);
        command->DrawInstanced(3, 1, 0, 0);
        Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        auto mainRtv = dxCommon_->GetRTVHeap()->GetCPUDescriptorHandleForHeapStart();
        mainRtv.ptr += static_cast<SIZE_T>(DirectXCommon::kRenderTextureRTVIndex) * dxCommon_->GetRTVDescriptorSize();
        const auto dsv = dxCommon_->GetDSVHeap()->GetCPUDescriptorHandleForHeapStart();
        command->OMSetRenderTargets(1, &mainRtv, FALSE, &dsv);
        command->RSSetViewports(1, &dxCommon_->GetViewport());
        command->RSSetScissorRects(1, &dxCommon_->GetScissorRect());
        command->SetGraphicsRootSignature(compositeRoot_.Get());
        command->SetPipelineState(compositePipeline_.Get());
        srvManager_->SetGraphicsRootDescriptorTable(0, textureSrv_);
        command->DrawInstanced(3, 1, 0, 0);
    }
private:
    void Transition(D3D12_RESOURCE_STATES next)
    {
        if (state_ == next) { return; }
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = texture_.Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = state_;
        barrier.Transition.StateAfter = next;
        dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
        state_ = next;
    }
    static constexpr LONG kWidth = 640, kHeight = 360;
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    uint32_t textureSrv_ = UINT32_MAX;
    D3D12_RESOURCE_STATES state_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
    Microsoft::WRL::ComPtr<ID3D12Resource> texture_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtv_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> compositeRoot_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> compositePipeline_;
};

TitleScene::TitleScene() = default;
TitleScene::~TitleScene() = default;

void TitleScene::Initialize()
{
    elapsed_ = menuRevealTime_ = departureTime_ = departureFade_ = departureAcceleration_ = 0.0f;
    selectedItem_ = 0;
    menuEmphasis_ = { 1.0f, 0.0f, 0.0f };
    showControls_ = startRequested_ = false;
    requestedScene_ = SceneType::Game;
}

void TitleScene::RequestStart(bool tutorial)
{
    if (startRequested_) { return; }
    requestedScene_ = tutorial ? SceneType::Tutorial : SceneType::Game;
    startRequested_ = true;
    showControls_ = false;
    if (sound_) { sound_->Play("confirm"); }
}

void TitleScene::PrepareBackdrop()
{
    if (objectCommon_ || GameScene::GetResourcePreloadStep() < 8) { return; }
    Model* model = ModelManager::GetInstance()->FindModel(kShip);
    if (!model || model->GetVertices().empty()) { return; }
    // 自機と環境光は本編と共有し、タイトルのカメラ・姿勢は独立させる。
    camera_ = std::make_unique<Camera>();
    camera_->SetFovY(0.56f);
    camera_->SetFarClip(200.0f);
    objectCommon_ = std::make_unique<Object3dCommon>();
    objectCommon_->Initialize(dxCommon_, srvManager_);
    objectCommon_->SetDefaultCamera(camera_.get());
    objectCommon_->SetEnvironmentTexturePath(kSky);

    Math::Vector3 min{ FLT_MAX, FLT_MAX, FLT_MAX }, max{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
    for (const auto& vertex : model->GetVertices()) {
        min.x = (std::min)(min.x, vertex.position.x); max.x = (std::max)(max.x, vertex.position.x);
        min.y = (std::min)(min.y, vertex.position.y); max.y = (std::max)(max.y, vertex.position.y);
        min.z = (std::min)(min.z, vertex.position.z); max.z = (std::max)(max.z, vertex.position.z);
    }
    modelCenter_ = { (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f };
    ship_ = std::make_unique<Object3d>();
    ship_->Initialize(objectCommon_.get());
    ship_->SetModel(model);
    ship_->SetLightingMode(2);
    ship_->SetColor({ 1.0f, 0.985f, 0.98f, 1.0f });
    ship_->SetDirectionalLightDirection({ -0.45f, -0.72f, 0.36f });
    ship_->SetDirectionalLightIntensity(1.5f);
    ship_->SetEnvironmentCoefficient(0.08f);
    ship_->SetShininess(112.0f);
    ship_->SetRoughness(0.38f);
    ship_->SetMetallic(0.24f);
    ship_->SetSpecularColor({ 0.30f, 0.32f, 0.35f });
    ship_->SetShadowReceiveStrength(0.0f);

    PrepareTitleComposition();

    // 本編で読み込み済みのエフェクトを再利用。汎用GPUパーティクルには接続しない。
    for (size_t index = 0; index < exhaust_.size(); ++index) {
        auto& effect = exhaust_[index];
        effect = std::make_unique<Object3d>();
        effect->Initialize(objectCommon_.get());
        effect->SetModel(ModelManager::GetInstance()->FindModel(index / 2 == 2 ?
            "effect_glow_core" : "effect_player_bullet_trail"));
        effect->SetLightingMode(0);
        effect->SetEnvironmentCoefficient(0.0f);
        effect->SetAlphaReference(0.003f);
    }
    sound_ = std::make_unique<SoundManager>();
    if (sound_->Initialize()) {
        sound_->Load("confirm", "resources/audio/combat/skill_ready.wav", 1, 0.22f);
        sound_->Load("launch", "resources/audio/combat/dodge.wav", 1, 0.48f);
    }
}

void TitleScene::PrepareTitleComposition()
{
    atmosphere_ = std::make_unique<TitleAtmosphere>();
    atmosphere_->Initialize(dxCommon_, srvManager_);
    wordmarkCommon_ = std::make_unique<ModelCommon>();
    wordmarkCommon_->Initialize(dxCommon_, srvManager_);
    wordmarkCommon_->SetEnvironmentTexturePath(kSky);
    for (size_t index = 0; index < wordmark_.size(); ++index) {
        wordmarkModels_[index] = std::make_unique<Model>();
        wordmarkModels_[index]->Initialize(wordmarkCommon_.get(), CreateWordmarkMesh(kTitle[index]));
        auto& letter = wordmark_[index];
        letter = std::make_unique<Object3d>();
        letter->Initialize(objectCommon_.get());
        letter->SetModel(wordmarkModels_[index].get());
        letter->SetLightingMode(2);
        letter->SetDirectionalLightDirection({ -0.18f, -0.36f, 0.92f });
        letter->SetDirectionalLightIntensity(1.12f);
        letter->SetRoughness(0.34f);
        letter->SetMetallic(0.30f);
        letter->SetShininess(128.0f);
        letter->SetSpecularColor({ 0.60f, 0.63f, 0.69f });
        letter->SetEnvironmentCoefficient(0.035f);
        letter->SetShadowReceiveStrength(0.0f);
    }
}

void TitleScene::UpdateBackdrop()
{
    if (!camera_) { return; }
    const float arrival = Smooth(elapsed_ / 1.5f);
    departureAcceleration_ = std::pow(Smooth((departureTime_ - 0.12f) / 1.9f), 2.0f);
    const float phase = elapsed_ * 0.24f;
    camera_->SetAspectRatio(dxCommon_->GetPresentationAspectRatio());
    camera_->SetFovY(0.62f + 0.065f * departureAcceleration_);
    camera_->SetTranslate({ 0, 0, -18 });
    camera_->SetRotate({ 0.04f + 0.006f * std::sin(phase * 0.61f),
        0.016f * std::sin(phase * 0.48f), -0.075f + 0.008f * std::sin(phase * 0.7f) });
    camera_->Update();
    const auto inView = [&](Math::Vector3 local) {
        const auto offset = RotateVector(local, camera_->GetWorldMatrix());
        const auto& position = camera_->GetTranslate();
        return Math::Vector3{ position.x + offset.x, position.y + offset.y, position.z + offset.z };
    };

    // ロゴも3D。機体が手前を通り、文字の厚みには照明が当たる。
    const auto cameraRotation = camera_->GetRotate();
    constexpr std::array<float, 6> advances{ 0, 76, 151, 230, 306, 332 };
    for (size_t index = 0; index < wordmark_.size(); ++index) {
        const float reveal = Smooth((elapsed_ - 0.25f - static_cast<float>(index) * 0.115f) / 0.62f);
        const float alpha = reveal * (1.0f - Smooth(departureTime_ / 0.28f));
        auto& letter = wordmark_[index];
        letter->SetTranslate(inView({ -10.6f + advances[index] * 0.028f * 1.35f,
            0.65f - 0.65f * (1.0f - reveal), 22.0f + 1.3f * (1.0f - reveal) }));
        letter->SetScale({ 1.35f, 1.35f, 1.35f });
        letter->SetRotate({ cameraRotation.x, cameraRotation.y - 0.035f - 0.48f * (1.0f - reveal),
            cameraRotation.z + 0.015f });
        letter->SetColor({ 1.0f, 0.985f, 0.95f, alpha });
        letter->SetSpotLightPosition(inView({ -8.5f + 18.0f * Smooth(elapsed_ / 2.1f), 4.0f, 18.0f }));
        letter->SetSpotLightDirection(RotateVector({ 0, -0.3f, 1.0f }, camera_->GetWorldMatrix()));
        letter->SetSpotLightIntensity(0.85f * (1.0f - Smooth((elapsed_ - 1.7f) / 0.6f)));
        letter->Update();
    }

    const float travel = 54.0f * departureAcceleration_;
    const Math::Vector3 center = inView({
        4.8f - 15.8f * (1.0f - arrival) + 0.20f * std::sin(phase) + travel * 0.15f,
        -1.0f + 3.4f * (1.0f - arrival) + 0.12f * std::sin(phase * 1.3f) + travel * 0.26f,
        17.8f - 8.0f * (1.0f - arrival) + travel });
    const Math::Vector3 rotation{ cameraRotation.x - 0.27f - 0.035f * std::sin(phase * 0.8f) - 0.10f * departureAcceleration_,
        cameraRotation.y - 0.60f + 1.6f * (1.0f - arrival) + 0.035f * std::sin(phase * 0.6f) + 0.48f * departureAcceleration_,
        cameraRotation.z - 0.10f + 0.10f * std::sin(phase) - 0.30f * (1.0f - arrival) - 0.17f * departureAcceleration_ };
    const auto rotationMatrix = Math::MakeAffineMatrix({ 1, 1, 1 }, rotation, {});
    const auto matrix = Math::MakeAffineMatrix({ kShipScale, kShipScale, kShipScale }, rotation, {});
    const auto offset = RotateVector(modelCenter_, matrix);
    ship_->SetScale({ kShipScale, kShipScale, kShipScale });
    ship_->SetRotate(rotation);
    ship_->SetTranslate({ center.x - offset.x, center.y - offset.y, center.z - offset.z });
    ship_->Update();
    UpdateFlightEffects(center, rotationMatrix, 2.6f + 4.5f * (1.0f - arrival) + 3.0f * departureAcceleration_);
}

void TitleScene::UpdateFlightEffects(const Math::Vector3& center, const Math::Matrix4x4& rotationMatrix, float thrust)
{
    const auto tail = RotateVector({ 0, 0, -1 }, rotationMatrix);
    const auto tailView = RotateVector(tail, camera_->GetViewMatrix());
    Math::Vector3 billboard = camera_->GetRotate();
    billboard.z += std::atan2(-tailView.x, tailView.y);
    const float pulse = 1.0f + 0.045f * std::sin(elapsed_ * 29.0f) + 0.025f * std::sin(elapsed_ * 43.0f);
    for (size_t index = 0; index < exhaust_.size(); ++index) {
        const int layer = static_cast<int>(index / 2);
        // 本編で確認済みのノズル位置をタイトルの表示倍率へ換算する。
        const float ratio = kShipScale / 1.26f;
        const auto nozzle = RotateVector({ (index % 2 == 0 ? -0.48f : 0.48f) * ratio,
            -0.28f * ratio, -1.70f * ratio }, rotationMatrix);
        const float length = (layer == 0 ? 1.25f : 0.78f) * thrust * pulse;
        const float centerOffset = layer == 2 ? 0.025f : length * 0.46f;
        auto& effect = exhaust_[index];
        effect->SetTranslate({ center.x + nozzle.x + tail.x * centerOffset,
            center.y + nozzle.y + tail.y * centerOffset, center.z + nozzle.z + tail.z * centerOffset });
        effect->SetRotate(billboard);
        if (layer == 2) {
            effect->SetScale({ 0.15f * pulse, 0.15f * pulse, 1.0f });
            effect->SetColor({ 0.60f, 0.78f, 1.0f, 0.58f });
        } else {
            effect->SetScale({ (layer == 0 ? 0.36f : 0.17f) * pulse, length, 1.0f });
            effect->SetColor(layer == 0 ? Math::Vector4{ 0.18f, 0.39f, 1.0f, 0.56f } : Math::Vector4{ 0.86f, 0.93f, 1.0f, 0.84f });
        }
        effect->Update();
    }
}

void TitleScene::Update()
{
    const float delta = dxCommon_ ? std::clamp(dxCommon_->GetDeltaTime(), 0.0f, 0.05f) : 1.0f / 60.0f;
    menuRevealTime_ = (std::min)(1.0f, menuRevealTime_ + delta);
    if (camera_) { elapsed_ += delta; }
    if (input_ && !startRequested_) {
        if (showControls_) {
            if (MenuUi::Pressed(input_, DIK_ESCAPE) || MenuUi::Pressed(input_, DIK_H) ||
                MenuUi::Pressed(input_, DIK_RETURN)) { showControls_ = false; }
        } else {
            if (MenuUi::Pressed(input_, DIK_UP) || MenuUi::Pressed(input_, DIK_W) || MenuUi::Pressed(input_, DIK_LEFT) || MenuUi::Pressed(input_, DIK_A)) { selectedItem_ = (selectedItem_ + 2) % 3; }
            if (MenuUi::Pressed(input_, DIK_DOWN) || MenuUi::Pressed(input_, DIK_S) || MenuUi::Pressed(input_, DIK_RIGHT) || MenuUi::Pressed(input_, DIK_D)) { selectedItem_ = (selectedItem_ + 1) % 3; }
            if (MenuUi::Pressed(input_, DIK_RETURN)) {
                if (selectedItem_ == 2) { showControls_ = true; }
                else { RequestStart(selectedItem_ == 1); }
            } else if (input_->TriggerKey(DIK_T)) { RequestStart(true); }
            else if (input_->TriggerKey(DIK_H)) { showControls_ = true; }
        }
    }
    const bool resourcesReady = GameScene::PreloadResourcesStep(dxCommon_, srvManager_);
    PrepareBackdrop();
    if (resourcesReady && sceneManager_ && !sceneManager_->IsScenePrepared(requestedScene_)) {
        sceneManager_->PrepareScene(requestedScene_);
    }
    const bool gameReady = sceneManager_ && sceneManager_->IsScenePrepared(requestedScene_);
    if (startRequested_ && gameReady && sceneManager_) {
        if (departureTime_ == 0.0f && sound_) { sound_->Play("launch"); }
        departureTime_ = (std::min)(kDepartureDuration, departureTime_ + delta);
        departureFade_ = Smooth((departureTime_ - 1.68f) / (kDepartureDuration - 1.68f));
        if (departureTime_ >= kDepartureDuration) { sceneManager_->SetNextScene(requestedScene_); }
    }
    UpdateBackdrop();
    DrawMenu(gameReady);
}

void TitleScene::DrawMenu(bool gameReady)
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float scale = CombatHud::Scale({ viewport->Size.x, viewport->Size.y });
    const ImVec2 origin(viewport->Pos.x + (viewport->Size.x - 1280.0f * scale) * 0.5f,
        viewport->Pos.y + (viewport->Size.y - 720.0f * scale) * 0.5f);
    const auto p = [&](float x, float y) { return ImVec2(origin.x + x * scale, origin.y + y * scale); };
    const ImVec2 end(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y);
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##Title", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (!atmosphere_) { draw->AddRectFilled(viewport->Pos, end, IM_COL32(8, 20, 36, 255)); }
    const float menuShade = 1.0f - Smooth(departureTime_ / 0.55f);
    draw->AddRectFilledMultiColor(p(0, 485), end, IM_COL32(0,0,0,0), IM_COL32(0,0,0,0),
        IM_COL32(4,9,22, static_cast<int>(155.0f * menuShade)),
        IM_COL32(4,9,22, static_cast<int>(155.0f * menuShade)));
    if (atmosphere_ && elapsed_ < 0.8f) {
        draw->AddRectFilled(viewport->Pos, end, IM_COL32(8, 20, 36,
            static_cast<int>(255.0f * (1.0f - Smooth(elapsed_ / 0.8f)))));
    }
    if (!showControls_) {
        const float reveal = Smooth(menuRevealTime_ / 0.85f);
        const float uiAlpha = reveal * Smooth((elapsed_ - 0.4f) / 0.65f) * (1.0f - Smooth(departureTime_ / 0.28f));
        const int firstMenuVertex = draw->VtxBuffer.Size;
        MenuUi::Text(draw, p(101, 363), 25.0f * scale, MenuUi::Paper, kTitleJapanese);

        const char* labels[] = { "出撃", "チュートリアル", "操作方法" };
        const float blend = 1.0f - std::exp(-12.0f * std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.05f));
        for (int index = 0; index < 3; ++index) {
            const float x = 92.0f + static_cast<float>(index) * 256.0f;
            const float y = 587.0f;
            ImGui::SetCursorScreenPos(p(x, y));
            ImGui::BeginDisabled(startRequested_);
            const bool clicked = ImGui::InvisibleButton(labels[index], { 228.0f * scale, 54.0f * scale });
            if (ImGui::IsItemHovered() && ImGui::IsMousePosValid() &&
                (ImGui::GetIO().MouseDelta.x != 0.0f || ImGui::GetIO().MouseDelta.y != 0.0f)) { selectedItem_ = index; }
            ImGui::EndDisabled();
            if (clicked) {
                selectedItem_ = index;
                if (index == 2) { showControls_ = true; }
                else { RequestStart(index == 1); }
            }
            const float target = selectedItem_ == index ? 1.0f : 0.0f;
            menuEmphasis_[index] += (target - menuEmphasis_[index]) * blend;
            const float emphasis = menuEmphasis_[index];
            const int alpha = static_cast<int>(245.0f * emphasis);
            const std::array<ImVec2, 4> button{ p(x, y), p(x + 228, y), p(x + 210, y + 54), p(x, y + 54) };
            draw->AddConvexPolyFilled(button.data(), static_cast<int>(button.size()),
                CombatHud::SurfaceColor(IM_COL32(243, 245, 248, alpha)));
            MenuUi::Text(draw, p(x + 26, y + 12), 25.0f * scale,
                CombatHud::Mix(IM_COL32(226, 234, 247, 255), MenuUi::Ink, emphasis), labels[index]);


        }
        if (!gameReady) {
            MenuUi::Text(draw, p(1184, 660), 16.0f * scale, MenuUi::Quiet, "読み込み中", true);
        }
        // ロゴとメニューだけをフェード。背景は飛行カットを最後まで見せる。
        for (int index = firstMenuVertex; index < draw->VtxBuffer.Size; ++index) {
            ImU32& color = draw->VtxBuffer[index].col;
            const auto alpha = static_cast<ImU32>(static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xff) * uiAlpha);
            color = (color & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
        }
    }
    if (departureFade_ > 0.0f) {
        draw->AddRectFilled(viewport->Pos, end, IM_COL32(0, 0, 0, static_cast<int>(255.0f * departureFade_)));
    }
    ImGui::End();
    ImGui::PopStyleVar();
    if (showControls_ && MenuUi::Controls(viewport->Pos, viewport->Size)) { showControls_ = false; }
}

void TitleScene::Draw()
{
    if (!atmosphere_) { return; }
    atmosphere_->Draw(*camera_, elapsed_, departureAcceleration_);
    objectCommon_->SetDepthDrawMode(DepthDrawMode::Normal);
    objectCommon_->CommonDrawSetting();
    // ロゴの裏面と側面にも自己遮蔽を適用し、手前の自機との前後関係を保つ。
    ship_->Draw();
    if (departureTime_ < 0.28f) {
        for (auto& letter : wordmark_) { if (letter->GetColor().w > 0.01f) { letter->Draw(); } }
    }
    objectCommon_->SetDepthDrawMode(DepthDrawMode::ReadOnly);
    objectCommon_->CommonDrawSetting();
    objectCommon_->SetBlendMode(BlendMode::Add);
    objectCommon_->CommonDrawSetting();
    for (auto& effect : exhaust_) { effect->Draw(); }
    objectCommon_->SetBlendMode(BlendMode::Normal);
    objectCommon_->SetDepthDrawMode(DepthDrawMode::Normal);
}

void TitleScene::Finalize()
{
    // フレーム終端のGPU完了待ち後にシーンが切り替わる。共有モデル・空は解放しない。
    ship_.reset();
    atmosphere_.reset();
    for (auto& letter : wordmark_) { letter.reset(); }
    for (auto& model : wordmarkModels_) { model.reset(); }
    wordmarkCommon_.reset();
    for (auto& effect : exhaust_) { effect.reset(); }
    sound_.reset();
    if (objectCommon_ && srvManager_) { srvManager_->Free(objectCommon_->GetShadowMapSrvIndex()); }
    objectCommon_.reset();
    camera_.reset();
}
