#include "engine/base/Framework.h"

#include "engine/base/WinApp.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/SrvManager.h"
#include "engine/base/ImGuiManager.h"
#include "engine/io/Input.h"
#include "engine/2d/SpriteCommon.h"
#include "engine/3d/TextureManager.h"
#include "engine/3d/ParticleManager.h"

Framework::~Framework() = default;

int Framework::Run() {
    Initialize();

    try {
        while (true) {
            Update();
            if (IsEndRequst()) { break; }
            Draw();
        }
    } catch (...) {
        Finalize();
        throw;
    }

    Finalize();
    return exitCode_;
}

void Framework::Initialize() {
    winApp_ = std::make_unique<WinApp>();
    winApp_->Initialize();

    dxCommon_ = std::make_unique<DirectXCommon>();
    dxCommon_->Initialize(winApp_.get());

    srvManager_ = std::make_unique<SrvManager>();
    srvManager_->Initialize(dxCommon_.get());
    dxCommon_->InitializeRenderTexture(srvManager_.get());

    imguiManager_ = std::make_unique<ImGuiManager>();
    imguiManager_->Initialize(winApp_.get(), dxCommon_.get(), srvManager_.get());

    TextureManager::GetInstance()->Initialize(dxCommon_.get(), srvManager_.get());

    input_ = std::make_unique<Input>();
    input_->Initialize(winApp_.get());

    spriteCommon_ = std::make_unique<SpriteCommon>();
    spriteCommon_->Initialize(dxCommon_.get(), srvManager_.get());

    ParticleManager::GetInstance()->Initialize(dxCommon_.get(), srvManager_.get());
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
    ParticleManager::GetInstance()->Finalize();
    TextureManager::GetInstance()->Destroy();

    if (imguiManager_) {
        imguiManager_->Finalize();
    }

    // GPU資源と入力デバイスを、依存先のDirectXとウィンドウより先に破棄する。
    spriteCommon_.reset();
    input_.reset();
    imguiManager_.reset();
    srvManager_.reset();
    dxCommon_.reset();

    if (winApp_) {
        winApp_->Finalize();
    }
    winApp_.reset();
}
