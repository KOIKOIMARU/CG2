#pragma once
#include <memory>

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

    std::unique_ptr<WinApp> winApp_;
    std::unique_ptr<DirectXCommon> dxCommon_;
    std::unique_ptr<SrvManager> srvManager_;
    std::unique_ptr<ImGuiManager> imguiManager_;
    std::unique_ptr<Input> input_;
    std::unique_ptr<SpriteCommon> spriteCommon_;
};
