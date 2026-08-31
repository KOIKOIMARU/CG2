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
    void Update(bool isEditorGuiVisible, bool showSkybox, int postEffectMode);
    void SetHudViewportRect(bool isEnabled, const Math::Vector2& min, const Math::Vector2& size);
    void Draw();
    int GetPostEffectMode() const;
    const Math::Matrix4x4* GetProjectionMatrix() const;
    bool IsRunning() const;
    bool IsExitRequested() const;

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    SpriteCommon* spriteCommon_ = nullptr;
    ImGuiManager* imguiManager_ = nullptr;
    Input* input_ = nullptr;
    RuntimeFactory runtimeFactory_;
    std::unique_ptr<IEditorPlayRuntime> runtime_;
};
