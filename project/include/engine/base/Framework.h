#pragma once
#include <memory>

#include "engine/io/Input.h"
#include "engine/2d/SpriteCommon.h"

class WinApp;
class DirectXCommon;
class SrvManager;
class ImGuiManager;

// アプリケーションの基本ライフサイクルを管理する基底クラス。
// ウィンドウ、DirectX 12、入力、ImGui、2D共通機能を所有し、Run から毎フレーム
// Update -> Draw を呼び出す。ゲーム側は継承して4つの仮想関数を実装する。
class Framework {
public:
    virtual ~Framework();

    // 終了要求が立つまでメインループを実行し、プロセス終了コードを返す。
    int Run();

    // 派生側でオーバーライドする場合も、共通基盤を準備するため基底実装を呼ぶこと。
    virtual void Initialize();
    virtual void Finalize();
    virtual void Update();
    virtual void Draw() = 0;

    virtual bool IsEndRequst() { return endRequst_; }

protected:
    bool endRequst_ = false;
    int exitCode_ = 0;

    std::unique_ptr<WinApp> winApp_;
    std::unique_ptr<DirectXCommon> dxCommon_;
    std::unique_ptr<SrvManager> srvManager_;
    std::unique_ptr<ImGuiManager> imguiManager_;
    std::unique_ptr<Input> input_;
    std::unique_ptr<SpriteCommon> spriteCommon_;
};
