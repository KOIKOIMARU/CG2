#include "engine/scene/TitleScene.h"
#include "app/CombatHud.h"
#include "engine/3d/Camera.h"
#include "engine/3d/ModelManager.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/3d/Skybox.h"
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

namespace {
// 仮題。正式名称が決まったら、この文字列だけ差し替える。
constexpr const char* kTitle = "SKYBREAK";
constexpr const char* kShip = "free_models/player_candidates/Omen.gltf";
constexpr const char* kSky = "resources/skybox/kloofendal_48d_partly_cloudy_puresky_4k_cube.dds";

void DrawWordmark(ImDrawList* draw, ImVec2 position, float scale)
{
    // 正体の書体と少し広い字間で組む。線・縁取り・副題は加えない。
    const float size = 116.0f * scale;
    ImFont* font = CombatHud::Font(true);
    for (const char* letter = kTitle; *letter; ++letter) {
        draw->AddText(font, size, position, IM_COL32(244, 245, 242, 255), letter, letter + 1);
        position.x += font->CalcTextSizeA(size, FLT_MAX, 0.0f, letter, letter + 1).x + 2.5f * scale;
    }
}
}

TitleScene::TitleScene() = default;
TitleScene::~TitleScene() = default;

void TitleScene::Initialize()
{
    elapsed_ = departureFade_ = 0.0f;
    selectedItem_ = 0;
    menuEmphasis_ = { 1.0f, 0.0f, 0.0f };
    showControls_ = startRequested_ = false;
    requestedScene_ = SceneType::Game;
}

void TitleScene::RequestStart(bool tutorial)
{
    requestedScene_ = tutorial ? SceneType::Tutorial : SceneType::Game;
    startRequested_ = true;
    showControls_ = false;
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
    ship_->SetColor({ 0.82f, 0.91f, 1.0f, 1.0f });
    ship_->SetDirectionalLightDirection({ -0.35f, -0.8f, -0.45f });
    ship_->SetDirectionalLightIntensity(1.1f);
    ship_->SetEnvironmentCoefficient(0.055f);
    ship_->SetRoughness(0.48f);
    ship_->SetMetallic(0.18f);
    ship_->SetSpecularColor({ 0.25f, 0.29f, 0.34f });
    ship_->SetShadowReceiveStrength(0.0f);
}

void TitleScene::UpdateBackdrop()
{
    if (!camera_) { return; }
    const auto size = dxCommon_->GetRenderTextureSize();
    camera_->SetAspectRatio(size.x / (std::max)(1.0f, size.y));
    camera_->SetTranslate({ 0.0f, 5.0f, -12.5f });
    camera_->SetRotate({ 0.29f, 0.006f * std::sin(elapsed_ * 0.12f), -0.025f });
    camera_->Update();
    // 機体は上から捉え、空は雲のある方角を見せる。モデルの投影には影響させない。
    Camera skyCamera = *camera_;
    skyCamera.SetRotate({ -0.12f, 0.18f + 0.02f * std::sin(elapsed_ * 0.08f), -0.025f });
    skyCamera.Update();
    skybox_->Update(&skyCamera);
    constexpr float shipScale = 1.82f;
    const float phase = elapsed_ * 0.32f;
    const Math::Vector3 rotation{ 0.0f, 0.72f + 0.018f * std::sin(phase * 0.7f), -0.12f + 0.012f * std::sin(phase) };
    const auto matrix = Math::MakeAffineMatrix({ shipScale, shipScale, shipScale }, rotation, {});
    const Math::Vector3 offset{
        modelCenter_.x * matrix.m[0][0] + modelCenter_.y * matrix.m[1][0] + modelCenter_.z * matrix.m[2][0],
        modelCenter_.x * matrix.m[0][1] + modelCenter_.y * matrix.m[1][1] + modelCenter_.z * matrix.m[2][1],
        modelCenter_.x * matrix.m[0][2] + modelCenter_.y * matrix.m[1][2] + modelCenter_.z * matrix.m[2][2] };
    ship_->SetScale({ shipScale, shipScale, shipScale });
    ship_->SetRotate(rotation);
    ship_->SetTranslate({ 3.2f - offset.x, 1.48f - offset.y + 0.07f * std::sin(phase), -offset.z });
    ship_->Update();
}

void TitleScene::Update()
{
    const float delta = dxCommon_ ? std::clamp(dxCommon_->GetDeltaTime(), 0.0f, 0.05f) : 1.0f / 60.0f;
    elapsed_ += delta;
    if (input_ && !startRequested_) {
        if (showControls_) {
            if (input_->TriggerKey(DIK_ESCAPE) || input_->TriggerKey(DIK_H)) { showControls_ = false; }
        } else {
            if (input_->TriggerKey(DIK_UP) || input_->TriggerKey(DIK_W)) { selectedItem_ = (selectedItem_ + 2) % 3; }
            if (input_->TriggerKey(DIK_DOWN) || input_->TriggerKey(DIK_S)) { selectedItem_ = (selectedItem_ + 1) % 3; }
            if (input_->TriggerKey(DIK_RETURN)) {
                if (selectedItem_ == 2) { showControls_ = true; }
                else { RequestStart(selectedItem_ == 1); }
            } else if (input_->TriggerKey(DIK_T)) { RequestStart(true); }
            else if (input_->TriggerKey(DIK_H)) { showControls_ = true; }
        }
    }
    const bool resourcesReady = GameScene::PreloadResourcesStep(dxCommon_, srvManager_);
    PrepareBackdrop();
    UpdateBackdrop();
    if (resourcesReady && sceneManager_ && !sceneManager_->IsScenePrepared(requestedScene_)) {
        sceneManager_->PrepareScene(requestedScene_);
    }
    const bool gameReady = sceneManager_ && sceneManager_->IsScenePrepared(requestedScene_);
    if (startRequested_ && gameReady && sceneManager_) {
        departureFade_ = (std::min)(1.0f, departureFade_ + delta / 0.45f);
        if (departureFade_ >= 1.0f) { sceneManager_->SetNextScene(requestedScene_); }
    }
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
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    // 背景を見せつつ、文字のある左側だけを暗くする。カードや説明欄は置かない。
    if (!skybox_) { draw->AddRectFilled(viewport->Pos, end, IM_COL32(8, 20, 36, 255)); }
    draw->AddRectFilledMultiColor(viewport->Pos, end, IM_COL32(9, 17, 28, 188), IM_COL32(9, 17, 28, 0),
        IM_COL32(9, 17, 28, 0), IM_COL32(9, 17, 28, 218));
    if (!showControls_) {
        DrawWordmark(draw, p(91, 164), scale);

        const char* labels[] = { "ゲーム開始", "操作練習", "操作方法" };
        const float blend = 1.0f - std::exp(-12.0f * std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.05f));
        for (int index = 0; index < 3; ++index) {
            const float y = 390.0f + static_cast<float>(index) * 52.0f;
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
            const int value = static_cast<int>(153.0f + 91.0f * menuEmphasis_[index]);
            CombatHud::Text(draw, p(95, y), 25.0f * scale, IM_COL32(value, value + 2, value + 3, 255), labels[index]);
        }
        if (!gameReady) {
            CombatHud::Text(draw, p(1184, 660), 16.0f * scale, CombatHud::Muted, "読み込み中", true);
        }
    }
    if (showControls_) {
        draw->AddRectFilled(viewport->Pos, end, IM_COL32(0, 6, 13, 210));
        CombatHud::Text(draw, p(340, 142), 32.0f * scale, CombatHud::White, "操作方法");
        const char* actions[] = { "移動", "照準", "射撃", "チャージ", "回避", "残像連撃" };
        const char* keys[] = { "WASD / 方向キー", "マウス", "SPACE 長押し", "SPACEを離してためる", "A・D + SHIFT", "Q" };
        for (int row = 0; row < 6; ++row) {
            const float y = 213.0f + static_cast<float>(row) * 46.0f;
            CombatHud::Text(draw, p(340, y), 21.0f * scale, CombatHud::Muted, actions[row]);
            CombatHud::Text(draw, p(940, y), 22.0f * scale, CombatHud::White, keys[row], true);
        }
        CombatHud::Text(draw, p(340, 519), 18.0f * scale, CombatHud::Muted, "フィーバーはゲージ満タンで自動発動");
        ImGui::SetCursorScreenPos(p(340, 574));
        if (ImGui::InvisibleButton("close_controls", { 600.0f * scale, 45.0f * scale })) { showControls_ = false; }
        CombatHud::Text(draw, p(340, 580), 22.0f * scale,
            ImGui::IsItemHovered() ? CombatHud::Blue : CombatHud::White, "戻る");
        CombatHud::Text(draw, p(940, 584), 20.0f * scale, CombatHud::Muted, "ESC", true, true);
    }
    if (departureFade_ > 0.0f) {
        draw->AddRectFilled(viewport->Pos, end, IM_COL32(0, 0, 0, static_cast<int>(255.0f * departureFade_)));
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void TitleScene::Draw()
{
    if (!skybox_) { return; }
    skybox_->Draw();
    objectCommon_->CommonDrawSetting();
    ship_->Draw();
}

void TitleScene::Finalize()
{
    // フレーム終端のGPU完了待ち後にシーンが切り替わる。共有モデル・空は解放しない。
    ship_.reset();
    skybox_.reset();
    if (objectCommon_ && srvManager_) { srvManager_->Free(objectCommon_->GetShadowMapSrvIndex()); }
    objectCommon_.reset();
    camera_.reset();
}
