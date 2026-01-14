#include "engine/game/MyGame.h"
#include "engine/base/D3DResourceLeakChecker.h"
#include <cassert>
#include <Windows.h>


void MyGame::Initialize() {
    // --- Win / DX ---
    winApp_ = new WinApp();
    winApp_->Initialize();

    dxCommon_ = new DirectXCommon();
    dxCommon_->Initialize(winApp_);

    srvManager_ = new SrvManager();
    srvManager_->Initialize(dxCommon_);

    imguiManager_ = new ImGuiManager();
    imguiManager_->Initialize(winApp_, dxCommon_, srvManager_);

    // ここでGetFenceEventしてCloseHandleしてたので保持
    fenceEvent_ = dxCommon_->GetFenceEvent();

    // --- Managers ---
    TextureManager::GetInstance()->Initialize(dxCommon_, srvManager_);

    object3dCommon_ = new Object3dCommon();
    object3dCommon_->Initialize(dxCommon_, srvManager_);

    modelCommon_ = new ModelCommon();
    modelCommon_->Initialize(dxCommon_, srvManager_);

    camera_ = new Camera();
    camera_->SetRotate({ 0.3f, 0.0f, 0.0f });
    camera_->SetTranslate({ 0.0f, 4.0f, -10.0f });
    object3dCommon_->SetDefaultCamera(camera_);

    ModelManager::GetInstance()->Initialize(dxCommon_, srvManager_);
    ModelManager::GetInstance()->LoadModel("plane.obj");

    object3d_ = new Object3d();
    object3d_->Initialize(object3dCommon_);
    object3d_->SetModel("plane.obj");

    // --- Sound ---
    sound_.Initialize();
    sound_.Load("alarm", "resources/Alarm01.mp3");

    // --- Input ---
    input_ = new Input();
    input_->Initialize(winApp_);

    // --- Sprite ---
    spriteCommon_ = new SpriteCommon();
    spriteCommon_->Initialize(dxCommon_, srvManager_);

    TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
    TextureManager::GetInstance()->LoadTexture("resources/monsterBall.png");
    TextureManager::GetInstance()->LoadTexture("resources/checkerBoard.png");

    sprites_.resize(1);
    sprites_[0].Initialize(spriteCommon_, "resources/uvChecker.png");
    sprites_[0].SetPosition(spritePos_);
    sprites_[0].SetSize({ 640, 360 });

    // --- Particle ---
    ParticleManager::GetInstance()->Initialize(dxCommon_, srvManager_);
    ParticleManager::GetInstance()->CreateParticleGroup("test", "resources/circle.png");

    emitter_ = new ParticleEmitter(
        "test",
        { 0.0f, 2.0f, 0.0f },
        0.1f,
        5
    );
}

void MyGame::Finalize() {
    // ループ終わったら後始末

    sound_.Finalize();

    delete emitter_;
    emitter_ = nullptr;

    delete object3d_;
    object3d_ = nullptr;

    delete camera_;
    camera_ = nullptr;

    delete spriteCommon_;
    spriteCommon_ = nullptr;

    delete input_;
    input_ = nullptr;

    // 依存が多いのでImGuiを先にFinalize
    if (imguiManager_) {
        imguiManager_->Finalize();
        delete imguiManager_;
        imguiManager_ = nullptr;
    }

    ParticleManager::GetInstance()->Finalize();
    TextureManager::GetInstance()->Destroy();
    ModelManager::GetInstance()->Finalize();

    delete object3dCommon_;
    object3dCommon_ = nullptr;

    delete modelCommon_;
    modelCommon_ = nullptr;

    delete srvManager_;
    srvManager_ = nullptr;

    // 君の元コード通り CloseHandle はここで
    if (fenceEvent_) {
        CloseHandle(fenceEvent_);
        fenceEvent_ = nullptr;
    }

    delete dxCommon_;
    dxCommon_ = nullptr;

    // WinApp最後
    if (winApp_) {
        winApp_->Finalize();
        delete winApp_;
        winApp_ = nullptr;
    }

#ifdef _DEBUG
    D3DResourceLeakChecker leakChecker;
#endif
}

void MyGame::Update() {
    // ★ここで終了判定（WinMainのProcessMessageを移植）
    if (winApp_->ProcessMessage()) {
        isEndRequest_ = true;
        return;
    }

    // --- ImGui ---
    imguiManager_->Begin();
    imguiManager_->ShowSpriteController(spritePos_);
    imguiManager_->End();

    sprites_[0].SetPosition(spritePos_);

    // --- Input ---
    input_->Update();

    if (input_->TriggerKey(DIK_SPACE)) {
        sound_.Play("alarm");
    }

    // --- Update ---
    float deltaTime = dxCommon_->GetDeltaTime();

    camera_->Update();
    object3d_->Update();

    emitter_->Update(deltaTime);

    ParticleManager::GetInstance()->Update(
        camera_->GetViewMatrix(),
        camera_->GetProjectionMatrix()
    );

    for (auto& s : sprites_) { s.Update(); }
}

void MyGame::Draw() {
    dxCommon_->PreDraw();
    srvManager_->PreDraw();

    ParticleManager::GetInstance()->Draw();

    object3dCommon_->CommonDrawSetting();
    object3d_->Draw();

    spriteCommon_->CommonDrawSetting();
    for (auto& s : sprites_) { s.Draw(); }

    imguiManager_->Draw();
    dxCommon_->PostDraw();
}
