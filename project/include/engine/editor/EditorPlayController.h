#pragma once

#include <functional>
#include <memory>

#include "engine/base/Math.h"
#include "engine/editor/IEditorPlayRuntime.h"

class DirectXCommon;
class ImGuiManager;
class Input;
class SpriteCommon;
class SrvManager;

// エディターとゲーム側ランタイムの生成・終了境界を管理する。
class EditorPlayController {
public:
    using RuntimeFactory =
        std::function<std::unique_ptr<IEditorPlayRuntime>()>;

    EditorPlayController();
    ~EditorPlayController();

    // アプリケーション側が実行ランタイムの生成方法を注入する。
    // 未設定の場合、Start は false を返しプレイモードを開始しない。
    void SetRuntimeFactory(RuntimeFactory runtimeFactory);
    void SetSystems(
        DirectXCommon* dxCommon,
        SrvManager* srvManager,
        SpriteCommon* spriteCommon,
        ImGuiManager* imguiManager,
        Input* input);
    bool Start();
    void Stop();
    void Update(bool showSkybox, int postEffectMode);
    void SetHudViewportRect(bool isEnabled, const Math::Vector2& min, const Math::Vector2& size);
    void Draw();
    int GetPostEffectMode() const;
    const Math::Matrix4x4* GetProjectionMatrix() const;
    bool IsRunning() const;
    bool IsExitRequested() const;

private:
    DirectXCommon* dxCommon_ = nullptr;     // ランタイムへ渡す描画基盤
    SrvManager* srvManager_ = nullptr;      // ランタイムへ渡すSRV管理
    SpriteCommon* spriteCommon_ = nullptr;  // ランタイムへ渡す2D描画基盤
    ImGuiManager* imguiManager_ = nullptr;  // ランタイムへ渡すUI基盤
    Input* input_ = nullptr;                // ランタイムへ渡す入力
    RuntimeFactory runtimeFactory_;         // アプリ側から注入された生成関数
    std::unique_ptr<IEditorPlayRuntime> runtime_; // Play中だけ所有する実行ランタイム
};
