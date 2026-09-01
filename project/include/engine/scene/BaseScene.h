#pragma once

class SceneManager;
class DirectXCommon;
class SrvManager;
class SpriteCommon;
class ImGuiManager;
class Input;

// SceneManager が所有するシーンの共通インターフェース。
// SetSystems で渡されるポインタは借用であり、シーン側で解放してはいけない。
class BaseScene {
public:
    virtual ~BaseScene() = default;

    virtual void Initialize() = 0;
    virtual void Finalize() = 0;
    virtual void Update() = 0;
    virtual void Draw() = 0;

    // シーン遷移を要求するときに利用する。所有権は SceneManager 側にある。
    virtual void SetSceneManager(SceneManager* sceneManager) {
        sceneManager_ = sceneManager;
    }

    // Framework が所有する共通システムをシーンへ注入する。
    virtual void SetSystems(
        DirectXCommon* dxCommon,
        SrvManager* srvManager,
        SpriteCommon* spriteCommon,
        ImGuiManager* imguiManager,
        Input* input
    ) {
        dxCommon_ = dxCommon;
        srvManager_ = srvManager;
        spriteCommon_ = spriteCommon;
        imguiManager_ = imguiManager;
        input_ = input;
    }

protected:
    SceneManager* sceneManager_ = nullptr;   // 遷移要求を受け付ける管理元
    DirectXCommon* dxCommon_ = nullptr;      // DirectX 12描画基盤の借用先
    SrvManager* srvManager_ = nullptr;       // SRV/UAV管理の借用先
    SpriteCommon* spriteCommon_ = nullptr;   // 2D共通描画の借用先
    ImGuiManager* imguiManager_ = nullptr;   // エディターUI基盤の借用先
    Input* input_ = nullptr;                 // 入力状態の借用先
};
