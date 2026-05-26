#pragma once

#include <memory>

class DirectXCommon;
class GameRuntime;
class ImGuiManager;
class Input;
class SpriteCommon;
class SrvManager;

class EditorPlayController {
public:
    EditorPlayController();
    ~EditorPlayController();

    void SetSystems(
        DirectXCommon* dxCommon,
        SrvManager* srvManager,
        SpriteCommon* spriteCommon,
        ImGuiManager* imguiManager,
        Input* input);
    bool Start();
    void Stop();
    void Update(bool isEditorGuiVisible);
    void Draw();
    int GetPostEffectMode() const;
    bool IsRunning() const;
    bool IsExitRequested() const;

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    SpriteCommon* spriteCommon_ = nullptr;
    ImGuiManager* imguiManager_ = nullptr;
    Input* input_ = nullptr;
    std::unique_ptr<GameRuntime> runtime_;
};
