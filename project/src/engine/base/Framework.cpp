#include "engine/base/Framework.h"

#include "engine/base/WinApp.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/SrvManager.h"
#include "engine/base/ImGuiManager.h"
#include "engine/io/Input.h"
#include "engine/2d/SpriteCommon.h"
#include "engine/3d/TextureManager.h"
#include "engine/3d/ParticleManager.h"

void Framework::Run() {
    Initialize();

    while (true) {
        Update();
        if (IsEndRequst()) { break; }
        Draw();
    }

    Finalize();
}

void Framework::Initialize() {
    winApp_ = new WinApp();
    winApp_->Initialize();

    dxCommon_ = new DirectXCommon();
    dxCommon_->Initialize(winApp_);

    srvManager_ = new SrvManager();
    srvManager_->Initialize(dxCommon_);

    imguiManager_ = new ImGuiManager();
    imguiManager_->Initialize(winApp_, dxCommon_, srvManager_);

    TextureManager::GetInstance()->Initialize(dxCommon_, srvManager_);

    input_ = new Input();
    input_->Initialize(winApp_);

    spriteCommon_ = new SpriteCommon();
    spriteCommon_->Initialize(dxCommon_, srvManager_);

    ParticleManager::GetInstance()->Initialize(dxCommon_, srvManager_);
}

void Framework::Update() {
    // Windowsメッセージ処理（終了フラグに変換）
    if (winApp_->ProcessMessage()) {
        endRequst_ = true;
        return;
    }

    input_->Update();
}

void Framework::Finalize() {
    // 基盤の後始末（ゲーム固有のFinalizeが先に呼ばれる前提）
    ParticleManager::GetInstance()->Finalize();
    TextureManager::GetInstance()->Destroy();

    delete spriteCommon_;
    spriteCommon_ = nullptr;

    delete input_;
    input_ = nullptr;

    if (imguiManager_) {
        imguiManager_->Finalize();
        delete imguiManager_;
        imguiManager_ = nullptr;
    }

    delete srvManager_;
    srvManager_ = nullptr;

    delete dxCommon_;
    dxCommon_ = nullptr;

    if (winApp_) {
        winApp_->Finalize();
        delete winApp_;
        winApp_ = nullptr;
    }
}
