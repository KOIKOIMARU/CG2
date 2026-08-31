#pragma once

#include "engine/base/Math.h"

class DirectXCommon;
class ImGuiManager;
class Input;
class SpriteCommon;
class SrvManager;

// エディタのプレイモードで実行するアプリケーション側ランタイムの境界。
// engine 層は具体的なゲームクラスを知らず、このインターフェースだけを操作する。
class IEditorPlayRuntime {
public:
    virtual ~IEditorPlayRuntime() = default;

    // Framework が所有する共通システムへの非所有ポインタを受け取る。
    virtual void SetSystems(
        DirectXCommon* dxCommon,
        SrvManager* srvManager,
        SpriteCommon* spriteCommon,
        ImGuiManager* imguiManager,
        Input* input) = 0;

    virtual void Initialize() = 0;
    virtual void Finalize() = 0;
    virtual void Update() = 0;
    virtual void Draw() = 0;

    virtual void SetHudViewportRect(
        bool isEnabled,
        const Math::Vector2& min,
        const Math::Vector2& size) = 0;
    virtual void SetRenderingOptions(bool showSkybox, int postEffectMode) = 0;
    virtual int GetPostEffectMode() const = 0;
    virtual const Math::Matrix4x4& GetProjectionMatrix() const = 0;
    virtual bool IsExitRequested() const = 0;
};
