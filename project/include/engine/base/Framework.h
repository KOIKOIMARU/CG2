#pragma once

class WinApp;
class DirectXCommon;
class SrvManager;
class ImGuiManager;
class Input;
class SpriteCommon;

class Framework {
public:
    virtual ~Framework() = default;

    void Run();

    virtual void Initialize();
    virtual void Finalize();
    virtual void Update();
    virtual void Draw() = 0;

    virtual bool IsEndRequst() { return endRequst_; }

protected:
    bool endRequst_ = false;

    // 汎用基盤（どのゲームでも使う）
    WinApp* winApp_ = nullptr;
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    ImGuiManager* imguiManager_ = nullptr;
    Input* input_ = nullptr;
    SpriteCommon* spriteCommon_ = nullptr;
};
