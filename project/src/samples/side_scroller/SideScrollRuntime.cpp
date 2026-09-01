#include "samples/side_scroller/SideScrollRuntime.h"

#include "engine/3d/Camera.h"
#include "engine/3d/Model.h"
#include "engine/3d/ModelManager.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/ImGuiManager.h"
#include "engine/base/SrvManager.h"
#include "engine/io/Input.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kGameplayDepth = 0.0f;
constexpr float kMoveSpeed = 6.0f;
constexpr float kJumpSpeed = 8.5f;
constexpr float kGravity = -22.0f;
constexpr float kMinimumPlayerY = -8.0f;
constexpr float kCameraFollowSpeed = 5.5f;
constexpr float kCameraLookAhead = 1.8f;

} // namespace

void SideScrollRuntime::SetSystems(
    DirectXCommon* dxCommon,
    SrvManager* srvManager,
    SpriteCommon* spriteCommon,
    ImGuiManager* imguiManager,
    Input* input)
{
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    spriteCommon_ = spriteCommon;
    imguiManager_ = imguiManager;
    input_ = input;
}

void SideScrollRuntime::Initialize()
{
    inputActions_.Initialize(input_);
    InitializeInputActions();

    object3dCommon_ = std::make_unique<Object3dCommon>();
    object3dCommon_->Initialize(dxCommon_, srvManager_);

    camera_ = std::make_unique<Camera>();
    camera_->SetRotate({ 0.08f, 0.0f, 0.0f });
    camera_->SetTranslate({ cameraX_, 3.7f, -18.0f });
    camera_->SetFovY(0.55f);
    camera_->SetNearClip(0.1f);
    camera_->SetFarClip(120.0f);
    camera_->SetOrthographicHeight(10.0f);
    camera_->Update();
    object3dCommon_->SetDefaultCamera(camera_.get());

    InitializeModels();
    InitializeWorld();
    ResetPlayer();
    UpdateObjectTransforms();
}

void SideScrollRuntime::Finalize()
{
    playerObject_.reset();
    for (Platform& platform : platforms_) {
        platform.object.reset();
    }
    for (auto& object : backdropObjects_) {
        object.reset();
    }
    camera_.reset();
    object3dCommon_.reset();

    dxCommon_ = nullptr;
    srvManager_ = nullptr;
    spriteCommon_ = nullptr;
    imguiManager_ = nullptr;
    input_ = nullptr;
}

void SideScrollRuntime::Update()
{
    const float deltaTime = dxCommon_ ?
        std::clamp(dxCommon_->GetDeltaTime(), 0.0f, 1.0f / 30.0f) :
        1.0f / 60.0f;

    if (inputActions_.IsTriggered(ToActionId(Action::Reset))) {
        ResetPlayer();
    }
    if (inputActions_.IsTriggered(ToActionId(Action::ToggleProjection))) {
        isOrthographic_ = !isOrthographic_;
        camera_->SetProjectionMode(
            isOrthographic_ ?
            CameraProjectionMode::Orthographic :
            CameraProjectionMode::Perspective);
    }

    UpdatePlayer(deltaTime);
    UpdateCamera(deltaTime);
    UpdateObjectTransforms();
    DrawGuideWindow();
}

void SideScrollRuntime::Draw()
{
    if (!object3dCommon_) {
        return;
    }

    object3dCommon_->CommonDrawSetting();
    for (const auto& object : backdropObjects_) {
        if (object) {
            object->Draw();
        }
    }
    for (const Platform& platform : platforms_) {
        if (platform.object) {
            platform.object->Draw();
        }
    }
    if (playerObject_) {
        playerObject_->Draw();
    }
}

void SideScrollRuntime::SetHudViewportRect(
    bool isEnabled,
    const Math::Vector2& min,
    const Math::Vector2& size)
{
    hasHudViewportRect_ = isEnabled && size.x > 1.0f && size.y > 1.0f;
    hudViewportMin_ = min;
    hudViewportSize_ = size;
}

void SideScrollRuntime::SetRenderingOptions(
    bool showSkybox,
    int postEffectMode)
{
    // 背景は簡易ボックスで構成しているため、Skybox設定は将来の拡張用として無視する。
    (void)showSkybox;
    postEffectMode_ = postEffectMode;
}

int SideScrollRuntime::GetPostEffectMode() const
{
    return postEffectMode_;
}

const Math::Matrix4x4& SideScrollRuntime::GetProjectionMatrix() const
{
    static const Math::Matrix4x4 kIdentity = Math::MakeIdentity4x4();
    return camera_ ? camera_->GetProjectionMatrix() : kIdentity;
}

bool SideScrollRuntime::IsExitRequested() const
{
    return false;
}

void SideScrollRuntime::InitializeInputActions()
{
    inputActions_.Bind(ToActionId(Action::MoveLeft), DIK_A, DIK_LEFT);
    inputActions_.Bind(ToActionId(Action::MoveRight), DIK_D, DIK_RIGHT);
    inputActions_.Bind(ToActionId(Action::Jump), DIK_SPACE, DIK_W);
    inputActions_.Bind(ToActionId(Action::Reset), DIK_R);
    inputActions_.Bind(ToActionId(Action::ToggleProjection), DIK_F5);
}

void SideScrollRuntime::InitializeModels()
{
    ModelManager* modelManager = ModelManager::GetInstance();
    modelManager->Initialize(dxCommon_, srvManager_);
    modelManager->CreateBox(
        "side_scroll_unit_box",
        1.0f,
        1.0f,
        1.0f,
        "resources/human/white.png");
}

void SideScrollRuntime::InitializeWorld()
{
    platforms_[0].center = { 0.0f, -0.5f, kGameplayDepth };
    platforms_[0].halfExtents = { 16.0f, 0.5f, 2.5f };
    // 左から順に、床からのジャンプ一回で届く高さへ配置する。
    platforms_[1].center = { -5.0f, 1.1f, kGameplayDepth };
    platforms_[1].halfExtents = { 2.0f, 0.25f, 2.0f };
    platforms_[2].center = { 2.5f, 2.2f, kGameplayDepth };
    platforms_[2].halfExtents = { 2.5f, 0.25f, 2.0f };
    platforms_[3].center = { 10.0f, 3.3f, kGameplayDepth };
    platforms_[3].halfExtents = { 2.0f, 0.25f, 2.0f };

    const std::array<Math::Vector4, kPlatformCount> platformColors{
        Math::Vector4{ 0.13f, 0.18f, 0.28f, 1.0f },
        Math::Vector4{ 0.35f, 0.62f, 0.86f, 1.0f },
        Math::Vector4{ 0.48f, 0.72f, 0.48f, 1.0f },
        Math::Vector4{ 0.72f, 0.52f, 0.82f, 1.0f },
    };

    for (size_t index = 0; index < platforms_.size(); ++index) {
        Platform& platform = platforms_[index];
        platform.object = CreateBoxObject(
            platform.center,
            {
                platform.halfExtents.x * 2.0f,
                platform.halfExtents.y * 2.0f,
                platform.halfExtents.z * 2.0f,
            },
            platformColors[index]);
    }

    for (size_t index = 0; index < backdropObjects_.size(); ++index) {
        const float x = -15.0f + static_cast<float>(index) * 5.0f;
        const float height = 2.5f + static_cast<float>((index * 3) % 4);
        backdropObjects_[index] = CreateBoxObject(
            { x, height * 0.5f, 4.5f },
            { 3.2f, height, 1.2f },
            { 0.10f, 0.16f + 0.025f * static_cast<float>(index), 0.28f, 1.0f });
    }

    playerObject_ = CreateBoxObject(
        playerPosition_,
        {
            playerHalfExtents_.x * 2.0f,
            playerHalfExtents_.y * 2.0f,
            playerHalfExtents_.z * 2.0f,
        },
        { 0.20f, 0.78f, 1.0f, 1.0f });
}

std::unique_ptr<Object3d> SideScrollRuntime::CreateBoxObject(
    const Math::Vector3& center,
    const Math::Vector3& size,
    const Math::Vector4& color)
{
    auto object = std::make_unique<Object3d>();
    object->Initialize(object3dCommon_.get());
    object->SetModel(ModelManager::GetInstance()->FindModel("side_scroll_unit_box"));
    object->SetTranslate(center);
    object->SetScale(size);
    object->SetColor(color);
    object->SetLightingMode(2);
    object->SetEnvironmentCoefficient(0.0f);
    object->SetDirectionalLightDirection({ 0.35f, -1.0f, 0.25f });
    object->SetDirectionalLightIntensity(1.15f);
    object->Update();
    return object;
}

void SideScrollRuntime::ResetPlayer()
{
    playerPosition_ = { -10.5f, 0.9f, kGameplayDepth };
    playerVelocity_ = {};
    cameraX_ = playerPosition_.x + 2.0f;
    isGrounded_ = true;
}

void SideScrollRuntime::UpdatePlayer(float deltaTime)
{
    float horizontalInput = 0.0f;
    if (inputActions_.IsPressed(ToActionId(Action::MoveLeft))) {
        horizontalInput -= 1.0f;
    }
    if (inputActions_.IsPressed(ToActionId(Action::MoveRight))) {
        horizontalInput += 1.0f;
    }

    const Math::Vector3 previousHorizontalPosition = playerPosition_;
    playerVelocity_.x = horizontalInput * kMoveSpeed;
    const float horizontalMove = playerVelocity_.x * deltaTime;
    playerPosition_.x += horizontalMove;
    playerPosition_.x = std::clamp(playerPosition_.x, -15.0f, 15.0f);
    ResolveHorizontalCollisions(previousHorizontalPosition, horizontalMove);

    if (isGrounded_ &&
        inputActions_.IsTriggered(ToActionId(Action::Jump))) {
        playerVelocity_.y = kJumpSpeed;
        isGrounded_ = false;
    }

    const Math::Vector3 previousVerticalPosition = playerPosition_;
    playerVelocity_.y += kGravity * deltaTime;
    playerPosition_.y += playerVelocity_.y * deltaTime;
    ResolveVerticalCollisions(previousVerticalPosition);

    // 2.5DではゲームプレイをX/Y平面に限定し、Z方向のずれを毎フレーム防ぐ。
    playerPosition_.z = kGameplayDepth;
    if (playerPosition_.y < kMinimumPlayerY) {
        ResetPlayer();
    }
}

void SideScrollRuntime::ResolveHorizontalCollisions(
    const Math::Vector3& previousPosition,
    float moveAmount)
{
    if (moveAmount == 0.0f) {
        return;
    }

    const Collision::Aabb previousBounds =
        Collision::MakeAabb(previousPosition, playerHalfExtents_);
    for (const Platform& platform : platforms_) {
        const Collision::Aabb platformBounds =
            Collision::MakeAabb(platform.center, platform.halfExtents);
        const Collision::Aabb playerBounds =
            Collision::MakeAabb(playerPosition_, playerHalfExtents_);
        if (!Collision::Intersects(playerBounds, platformBounds)) {
            continue;
        }

        if (moveAmount > 0.0f &&
            previousBounds.max.x <= platformBounds.min.x + 0.001f) {
            playerPosition_.x = platformBounds.min.x - playerHalfExtents_.x;
        } else if (moveAmount < 0.0f &&
                   previousBounds.min.x >= platformBounds.max.x - 0.001f) {
            playerPosition_.x = platformBounds.max.x + playerHalfExtents_.x;
        }
    }
}

void SideScrollRuntime::ResolveVerticalCollisions(
    const Math::Vector3& previousPosition)
{
    isGrounded_ = false;
    const Collision::Aabb previousBounds =
        Collision::MakeAabb(previousPosition, playerHalfExtents_);

    for (const Platform& platform : platforms_) {
        const Collision::Aabb platformBounds =
            Collision::MakeAabb(platform.center, platform.halfExtents);
        Collision::Aabb playerBounds =
            Collision::MakeAabb(playerPosition_, playerHalfExtents_);
        if (!Collision::Intersects(playerBounds, platformBounds) ||
            !Collision::OverlapsXZ(playerBounds, platformBounds)) {
            continue;
        }

        if (playerVelocity_.y <= 0.0f &&
            previousBounds.min.y >= platformBounds.max.y - 0.02f) {
            playerPosition_.y = platformBounds.max.y + playerHalfExtents_.y;
            playerVelocity_.y = 0.0f;
            isGrounded_ = true;
        } else if (playerVelocity_.y > 0.0f &&
                   previousBounds.max.y <= platformBounds.min.y + 0.02f) {
            playerPosition_.y = platformBounds.min.y - playerHalfExtents_.y;
            playerVelocity_.y = 0.0f;
        }
    }
}

void SideScrollRuntime::UpdateCamera(float deltaTime)
{
    const float moveDirection =
        playerVelocity_.x > 0.01f ? 1.0f :
        playerVelocity_.x < -0.01f ? -1.0f : 0.0f;
    const float targetX = playerPosition_.x +
        (moveDirection == 0.0f ? 2.0f : moveDirection * kCameraLookAhead);
    const float blend = (std::min)(1.0f, kCameraFollowSpeed * deltaTime);
    cameraX_ += (targetX - cameraX_) * blend;

    camera_->SetTranslate({ cameraX_, 3.7f, -18.0f });
    camera_->Update();
}

void SideScrollRuntime::UpdateObjectTransforms()
{
    if (playerObject_) {
        playerObject_->SetTranslate(playerPosition_);
        playerObject_->Update();
    }
    for (Platform& platform : platforms_) {
        if (platform.object) {
            platform.object->Update();
        }
    }
    for (auto& object : backdropObjects_) {
        if (object) {
            object->Update();
        }
    }
}

void SideScrollRuntime::DrawGuideWindow()
{
#ifdef USE_IMGUI
    ImGui::SetNextWindowBgAlpha(0.88f);
    const ImVec2 guidePosition = hasHudViewportRect_ ?
        ImVec2(hudViewportMin_.x + 12.0f, hudViewportMin_.y + 12.0f) :
        ImVec2(18.0f, 72.0f);
    ImGui::SetNextWindowPos(guidePosition, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_Always);
    constexpr ImGuiWindowFlags kGuideFlags =
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_AlwaysAutoResize;
    if (ImGui::Begin("2.5D Side-Scroll Starter", nullptr, kGuideFlags)) {
        ImGui::TextUnformatted("X: 横 / Y: 高さ / Z: 奥行き（プレイヤーはZ=0固定）");
        ImGui::Separator();
        ImGui::TextUnformatted("A・D / 左右キー : 移動");
        ImGui::TextUnformatted("Space・W : ジャンプ");
        ImGui::TextUnformatted("R : 初期位置へ戻す");
        ImGui::TextUnformatted("F5 : 透視投影 / 正投影を切り替え");
        ImGui::Separator();
        ImGui::Text("Position  X %.2f  Y %.2f  Z %.2f",
            playerPosition_.x,
            playerPosition_.y,
            playerPosition_.z);
        ImGui::Text("Grounded  %s", isGrounded_ ? "true" : "false");
        ImGui::Text("Projection  %s",
            isOrthographic_ ? "Orthographic" : "Perspective");
    }
    ImGui::End();
#endif
}

InputActionMap::ActionId SideScrollRuntime::ToActionId(Action action)
{
    return static_cast<InputActionMap::ActionId>(action);
}
