#pragma once

#include "engine/base/Math.h"
#include "engine/collision/Collision.h"
#include "engine/editor/IEditorPlayRuntime.h"
#include "engine/io/InputActionMap.h"

#include <array>
#include <cstdint>
#include <memory>

class Camera;
class DirectXCommon;
class ImGuiManager;
class Input;
class Object3d;
class Object3dCommon;
class SpriteCommon;
class SrvManager;

// 3DモデルをX/Y平面上で操作する2.5D横スクロールの最小動作例。
// チーム作品はこの責務分離を参考にし、ゲーム固有処理をengine層へ入れない。
class SideScrollRuntime final : public IEditorPlayRuntime {
public:
    SideScrollRuntime() = default;
    ~SideScrollRuntime() override = default;

    void SetSystems(
        DirectXCommon* dxCommon,
        SrvManager* srvManager,
        SpriteCommon* spriteCommon,
        ImGuiManager* imguiManager,
        Input* input) override;
    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;

    void SetHudViewportRect(
        bool isEnabled,
        const Math::Vector2& min,
        const Math::Vector2& size) override;
    void SetRenderingOptions(bool showSkybox, int postEffectMode) override;
    int GetPostEffectMode() const override;
    const Math::Matrix4x4& GetProjectionMatrix() const override;
    bool IsExitRequested() const override;

private:
    enum class Action : uint32_t {
        MoveLeft,
        MoveRight,
        Jump,
        Reset,
        ToggleProjection,
    };

    struct Platform {
        std::unique_ptr<Object3d> object;
        Math::Vector3 center{};
        Math::Vector3 halfExtents{};
    };

    static constexpr size_t kPlatformCount = 4;
    static constexpr size_t kBackdropCount = 7;

    void InitializeInputActions();
    void InitializeModels();
    void InitializeWorld();
    std::unique_ptr<Object3d> CreateBoxObject(
        const Math::Vector3& center,
        const Math::Vector3& size,
        const Math::Vector4& color);

    void ResetPlayer();
    void UpdatePlayer(float deltaTime);
    void ResolveHorizontalCollisions(
        const Math::Vector3& previousPosition,
        float moveAmount);
    void ResolveVerticalCollisions(
        const Math::Vector3& previousPosition);
    void UpdateCamera(float deltaTime);
    void UpdateObjectTransforms();
    void DrawGuideWindow();

    static InputActionMap::ActionId ToActionId(Action action);

    // Frameworkが所有するシステムへの非所有ポインタ。
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    SpriteCommon* spriteCommon_ = nullptr;
    ImGuiManager* imguiManager_ = nullptr;
    Input* input_ = nullptr;

    InputActionMap inputActions_;
    std::unique_ptr<Object3dCommon> object3dCommon_;
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<Object3d> playerObject_;
    std::array<Platform, kPlatformCount> platforms_{};
    std::array<std::unique_ptr<Object3d>, kBackdropCount> backdropObjects_{};

    Math::Vector3 playerPosition_{ -10.5f, 0.9f, 0.0f };
    Math::Vector3 playerVelocity_{};
    Math::Vector3 playerHalfExtents_{ 0.5f, 0.9f, 0.5f };
    float cameraX_ = -8.5f;
    bool isGrounded_ = false;
    bool isOrthographic_ = false;
    bool hasHudViewportRect_ = false;
    Math::Vector2 hudViewportMin_{};
    Math::Vector2 hudViewportSize_{};
    int postEffectMode_ = 0;
};
