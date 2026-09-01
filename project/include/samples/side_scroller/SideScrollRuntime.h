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
    SideScrollRuntime();
    ~SideScrollRuntime() override;

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
        MoveLeft,         // 左へ移動
        MoveRight,        // 右へ移動
        Jump,             // 接地中にジャンプ
        Reset,            // 初期位置へ戻す
        ToggleProjection, // 透視投影と平行投影を切り替える
    };

    struct Platform {
        std::unique_ptr<Object3d> object; // 足場の描画オブジェクト
        Math::Vector3 center{};           // 当たり判定のワールド中心
        Math::Vector3 halfExtents{};      // 当たり判定の各軸半サイズ
    };

    static constexpr size_t kPlatformCount = 4; // サンプルに固定配置する足場数
    static constexpr size_t kBackdropCount = 7; // 背景を構成する箱の数

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

    DirectXCommon* dxCommon_ = nullptr;     // 描画基盤の借用先
    SrvManager* srvManager_ = nullptr;      // SRV管理の借用先
    SpriteCommon* spriteCommon_ = nullptr;  // 2D共通描画の借用先
    ImGuiManager* imguiManager_ = nullptr;  // ガイドUI描画の借用先
    Input* input_ = nullptr;                // 操作入力の借用先

    InputActionMap inputActions_;                    // サンプル操作と物理キーの対応表
    std::unique_ptr<Object3dCommon> object3dCommon_; // 3Dオブジェクトの共有描画設定
    std::unique_ptr<Camera> camera_;                  // サンプル空間を映すカメラ
    std::unique_ptr<Object3d> playerObject_;          // 操作対象の箱モデル
    std::array<Platform, kPlatformCount> platforms_{};// 描画と判定を持つ全足場
    std::array<std::unique_ptr<Object3d>, kBackdropCount> backdropObjects_{}; // 背景用オブジェクト

    Math::Vector3 playerPosition_{ -10.5f, 0.9f, 0.0f }; // プレイヤー中心のワールド位置
    Math::Vector3 playerVelocity_{};                      // 重力を含む1秒あたりの移動量
    Math::Vector3 playerHalfExtents_{ 0.5f, 0.9f, 0.5f };// プレイヤーAABBの各軸半サイズ
    float cameraX_ = -8.5f;                               // プレイヤーを追従するカメラのX座標
    bool isGrounded_ = false;                             // 足場上でジャンプ可能か
    bool isOrthographic_ = false;                         // 現在、平行投影を使っているか
    bool hasHudViewportRect_ = false;                     // エディター内の描画範囲が指定済みか
    Math::Vector2 hudViewportMin_{};                       // 描画範囲の左上スクリーン座標
    Math::Vector2 hudViewportSize_{};                      // 描画範囲の幅と高さ
    int postEffectMode_ = 0;                              // DirectXCommonへ渡すポストエフェクト番号
};
