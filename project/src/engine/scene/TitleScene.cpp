#include "engine/scene/TitleScene.h"
#include "app/CombatHud.h"
#include "app/MenuUi.h"
#include "engine/3d/Camera.h"
#include "engine/3d/ModelManager.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/3d/Skybox.h"
#include "engine/audio/SoundManager.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/SrvManager.h"
#include "engine/io/Input.h"
#include "engine/scene/GameScene.h"
#include "engine/scene/SceneManager.h"
#include <dinput.h>
#include <DirectXMath.h>
#include <imgui.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <numbers>

namespace {
// 仮題。正式名称が決まったら、この文字列だけ差し替える。
constexpr const char* kTitleTop = "SKY";
constexpr const char* kTitleBottom = "BREAK";
constexpr const char* kShip = "free_models/player_candidates/Omen.gltf";
constexpr const char* kSky = "resources/skybox/kloofendal_48d_partly_cloudy_puresky_4k_cube.dds";
constexpr float kShipScale = 1.96f;
constexpr float kDepartureDuration = 1.25f;

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

void DrawWordmark(ImDrawList* draw, ImVec2 position, float scale)
{
    // 二段の角形ロゴ。文字の傾き・字間・縦の濃淡を組版側で揃える。
    // 既存のフォント頂点だけを変形し、追加テクスチャや描画経路は使わない。
    const float size = 152.0f * scale;
    ImFont* font = CombatHud::Font(true);
    for (int row = 0; row < 2; ++row) {
        ImVec2 pen{ position.x, position.y + static_cast<float>(row) * 104.0f * scale };
        const char* word = row == 0 ? kTitleTop : kTitleBottom;
        const int firstVertex = draw->VtxBuffer.Size;
        for (const char* letter = word; *letter; ++letter) {
            draw->AddText(font, size, pen, MenuUi::Paper, letter, letter + 1);
            pen.x += font->CalcTextSizeA(size, FLT_MAX, 0.0f, letter, letter + 1).x - 1.5f * scale;
        }
        for (int index = firstVertex; index < draw->VtxBuffer.Size; ++index) {
            auto& vertex = draw->VtxBuffer[index];
            const float y = vertex.pos.y - (position.y + static_cast<float>(row) * 104.0f * scale);
            vertex.pos.x += (size * 0.75f - y) * 0.18f;
            const float gradient = std::clamp(y / size, 0.0f, 1.0f);
            const ImU32 top = IM_COL32(248, 247, 244, 255);
            const ImU32 bottom = IM_COL32(188, 192, 198, 255);
            vertex.col = CombatHud::SurfaceColor(CombatHud::Mix(top, bottom, gradient * 0.52f));
        }
    }
}
}

TitleScene::TitleScene() = default;
TitleScene::~TitleScene() = default;

void TitleScene::Initialize()
{
    elapsed_ = menuRevealTime_ = departureTime_ = departureFade_ = 0.0f;
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
    // すでに本編用に読んだモデルと空を借りる。タイトル用に再読み込みしない。
    camera_ = std::make_unique<Camera>();
    camera_->SetFovY(0.56f);
    camera_->SetFarClip(200.0f);
    objectCommon_ = std::make_unique<Object3dCommon>();
    objectCommon_->Initialize(dxCommon_, srvManager_);
    objectCommon_->SetDefaultCamera(camera_.get());
    objectCommon_->SetEnvironmentTexturePath(kSky);
    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(dxCommon_, srvManager_, kSky);

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
    ship_->SetLightingMode(1);
    ship_->SetColor({ 0.96f, 0.97f, 1.0f, 1.0f });
    ship_->SetDirectionalLightDirection({ -0.35f, -0.8f, -0.45f });
    ship_->SetDirectionalLightIntensity(1.1f);
    ship_->SetEnvironmentCoefficient(0.055f);
    ship_->SetRoughness(0.48f);
    ship_->SetMetallic(0.18f);
    ship_->SetSpecularColor({ 0.25f, 0.29f, 0.34f });
    ship_->SetShadowReceiveStrength(0.0f);

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

void TitleScene::UpdateBackdrop()
{
    if (!camera_) { return; }
    const float arrival = Smooth(elapsed_ / 2.4f);
    const float departure = Smooth(departureTime_ / kDepartureDuration);
    const float acceleration = std::pow(std::clamp((departureTime_ - 0.16f) / 0.94f, 0.0f, 1.0f), 2.0f);
    const float phase = elapsed_ * 0.38f;
    // 展示中の短い見せ場。導入から9秒後、その後は22秒周期で一回だけロールする。
    // 一周の終端は2πと0が同じ姿勢なので、周期境界でも向きが跳ばない。
    const float rollTime = elapsed_ < 9.0f ? 0.0f : std::fmod(elapsed_ - 9.0f, 22.0f);
    const float rollProgress = Smooth(rollTime / 2.1f);
    const float rollAngle = 2.0f * std::numbers::pi_v<float> * rollProgress;
    const float rollLift = std::sin(std::numbers::pi_v<float> * rollProgress);
    camera_->SetAspectRatio(dxCommon_->GetPresentationAspectRatio());
    camera_->SetFovY(0.56f + 0.045f * (1.0f - arrival) + 0.09f * departure);
    camera_->SetTranslate({ 0.24f * std::sin(phase * 0.61f),
        5.0f + 0.55f * (1.0f - arrival) + 0.10f * std::sin(phase * 0.8f),
        -12.5f - 1.8f * (1.0f - arrival) });
    camera_->SetRotate({ 0.29f + 0.008f * std::sin(phase * 0.8f),
        0.012f * std::sin(phase * 0.61f), -0.025f + 0.008f * std::sin(phase) });
    camera_->Update();
    // 機体は上から捉え、空は雲のある方角を見せる。モデルの投影には影響させない。
    Camera skyCamera = *camera_;
    // 雲の流れを機体より遅くして遠景の奥行きを作る。周期の端で巻き戻さない。
    skyCamera.SetRotate({ -0.12f + 0.018f * std::sin(phase * 0.47f),
        0.18f + elapsed_ * 0.018f + 0.12f * acceleration, camera_->GetRotate().z });
    skyCamera.Update();
    skybox_->Update(&skyCamera);
    const Math::Vector3 rotation{ -0.035f * std::sin(phase * 0.7f) - 0.14f * departure,
        0.84f + 0.14f * std::sin(phase * 0.7f) - 0.20f * (1.0f - arrival),
        -0.18f + 0.11f * std::sin(phase) - 0.30f * departure };
    // ワールドZではなく機首の軸で回す。先にローカルロール、続けて通常の飛行姿勢を合成する。
    DirectX::XMFLOAT4 quaternion{};
    DirectX::XMStoreFloat4(&quaternion, DirectX::XMQuaternionRotationMatrix(
        DirectX::XMMatrixRotationZ(rollAngle) * DirectX::XMMatrixRotationX(rotation.x) *
        DirectX::XMMatrixRotationY(rotation.y) * DirectX::XMMatrixRotationZ(rotation.z)));
    const Math::Quaternion orientation{ quaternion.x, quaternion.y, quaternion.z, quaternion.w };
    const auto rotationMatrix = Math::MakeRotateMatrix(orientation);
    const auto matrix = Math::MakeAffineMatrix({ kShipScale, kShipScale, kShipScale }, orientation, {});
    const auto offset = RotateVector(modelCenter_, matrix);
    // 機首方向(+Z)へ加速させる。縮小だけで消さず、実際に空の奥へ飛び去る。
    const auto forward = RotateVector({ 0.0f, 0.0f, 1.0f }, rotationMatrix);
    // ロール中だけ奥へ引き、縦になった翼も画面内に収める。
    const Math::Vector3 center{ 3.6f + 5.5f * (1.0f - arrival) + 0.22f * std::sin(phase * 0.7f) + forward.x * acceleration * 30.0f,
        1.66f + 0.9f * (1.0f - arrival) + 0.18f * std::sin(phase) - 0.20f * rollLift + 3.2f * acceleration,
        6.0f * (1.0f - arrival) + 2.8f * rollLift + forward.z * acceleration * 30.0f };
    ship_->SetScale({ kShipScale, kShipScale, kShipScale });
    ship_->SetQuaternionRotate(orientation);
    ship_->SetTranslate({ center.x - offset.x, center.y - offset.y, center.z - offset.z });
    ship_->Update();
    UpdateFlightEffects(center, rotationMatrix, 1.0f + 0.35f * rollLift + 2.4f * departure);
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
            if (MenuUi::Pressed(input_, DIK_UP) || MenuUi::Pressed(input_, DIK_W)) { selectedItem_ = (selectedItem_ + 2) % 3; }
            if (MenuUi::Pressed(input_, DIK_DOWN) || MenuUi::Pressed(input_, DIK_S)) { selectedItem_ = (selectedItem_ + 1) % 3; }
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
        departureFade_ = Smooth((departureTime_ - 0.77f) / (kDepartureDuration - 0.77f));
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
    // 背景を見せつつ、文字のある左側だけを暗くする。カードや説明欄は置かない。
    if (!skybox_) { draw->AddRectFilled(viewport->Pos, end, IM_COL32(8, 20, 36, 255)); }
    const ImU32 shade = CombatHud::SurfaceColor(IM_COL32(13, 16, 22, 228));
    draw->AddRectFilledMultiColor(viewport->Pos, end, shade, IM_COL32(0, 0, 0, 0),
        IM_COL32(0, 0, 0, 0), shade);
    draw->AddRectFilledMultiColor(p(0, 490), end, IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0),
        IM_COL32(0, 0, 0, 112), IM_COL32(0, 0, 0, 112));
    if (!showControls_) {
        const float reveal = Smooth(menuRevealTime_ / 0.85f);
        const float uiAlpha = reveal * (1.0f - Smooth(departureTime_ / 0.28f));
        const int firstMenuVertex = draw->VtxBuffer.Size;
        DrawWordmark(draw, p(90 + 14.0f * (1.0f - reveal), 106), scale);

        const char* labels[] = { "出撃", "チュートリアル", "操作方法" };
        const float blend = 1.0f - std::exp(-12.0f * std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.05f));
        for (int index = 0; index < 3; ++index) {
            const float y = 450.0f + static_cast<float>(index) * 54.0f;
            ImGui::SetCursorScreenPos(p(88, y - 6.0f));
            ImGui::BeginDisabled(startRequested_);
            const bool clicked = ImGui::InvisibleButton(labels[index], { 250.0f * scale, 44.0f * scale });
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
            draw->AddRectFilledMultiColor(p(88, y - 6), p(338, y + 38),
                CombatHud::SurfaceColor(IM_COL32(239, 241, 244, alpha)),
                CombatHud::SurfaceColor(IM_COL32(214, 215, 219, alpha)),
                CombatHud::SurfaceColor(IM_COL32(194, 197, 202, alpha)),
                CombatHud::SurfaceColor(IM_COL32(229, 231, 234, alpha)));
            MenuUi::Text(draw, p(110, y + 2), 23.0f * scale,
                CombatHud::Mix(MenuUi::Quiet, MenuUi::Ink, emphasis), labels[index]);
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
    if (!skybox_) { return; }
    skybox_->Draw();
    objectCommon_->CommonDrawSetting();
    ship_->Draw();
    objectCommon_->SetBlendMode(BlendMode::Add);
    objectCommon_->SetDepthDrawMode(DepthDrawMode::ReadOnly);
    objectCommon_->CommonDrawSetting();
    for (auto& effect : exhaust_) { effect->Draw(); }
    objectCommon_->SetBlendMode(BlendMode::Normal);
    objectCommon_->SetDepthDrawMode(DepthDrawMode::Normal);
}

void TitleScene::Finalize()
{
    // フレーム終端のGPU完了待ち後にシーンが切り替わる。共有モデル・空は解放しない。
    ship_.reset();
    for (auto& effect : exhaust_) { effect.reset(); }
    sound_.reset();
    skybox_.reset();
    if (objectCommon_ && srvManager_) { srvManager_->Free(objectCommon_->GetShadowMapSrvIndex()); }
    objectCommon_.reset();
    camera_.reset();
}
