#pragma once

class SceneManager;
class DirectXCommon;
class SrvManager;
class SpriteCommon;
class ImGuiManager;

class BaseScene {
public:
    virtual ~BaseScene() = default;

    virtual void Initialize() = 0;
    virtual void Finalize() = 0;
    virtual void Update() = 0;
    virtual void Draw() = 0;

    virtual void SetSceneManager(SceneManager* sceneManager) {
        sceneManager_ = sceneManager;
    }

    virtual void SetSystems(
        DirectXCommon* dxCommon,
        SrvManager* srvManager,
        SpriteCommon* spriteCommon,
        ImGuiManager* imguiManager
    ) {
        dxCommon_ = dxCommon;
        srvManager_ = srvManager;
        spriteCommon_ = spriteCommon;
        imguiManager_ = imguiManager;
    }

protected:
    SceneManager* sceneManager_ = nullptr;

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    SpriteCommon* spriteCommon_ = nullptr;
    ImGuiManager* imguiManager_ = nullptr;
};