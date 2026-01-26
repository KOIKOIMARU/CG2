#include "app/MyGame.h"

#include "engine/3d/Object3dCommon.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Camera.h"
#include "engine/2d/Sprite.h"
#include "engine/io/Input.h"
#include "engine/2d/SpriteCommon.h"
#include <dinput.h>
#include "engine/3d/TextureManager.h"
#include "engine/3d/ParticleManager.h"
#include "engine/3d/ParticleEmitter.h"
#include "engine/audio/SoundManager.h"
#include "engine/base/ImGuiManager.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/SrvManager.h"
#include "engine/3d/ModelManager.h"

void MyGame::Initialize() {
    // ★汎用初期化
    Framework::Initialize();

    // ===== 3D共通部 =====
    object3dCommon_ = new Object3dCommon();
    object3dCommon_->Initialize(dxCommon_, srvManager_);

    // ===== カメラ =====
    camera_ = new Camera();
    camera_->SetRotate({ 0.3f, 0.0f, 0.0f });
    camera_->SetTranslate({ 0.0f, 4.0f, -10.0f });
    object3dCommon_->SetDefaultCamera(camera_);

    // ===== モデル =====
    ModelManager::GetInstance()->Initialize(dxCommon_, srvManager_);
    ModelManager::GetInstance()->LoadModel("plane.obj");

    object3d_ = new Object3d();
    object3d_->Initialize(object3dCommon_);
    object3d_->SetModel("plane.obj");

    // ----- テクスチャ読み込み（ゲーム固有） -----
    TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
    TextureManager::GetInstance()->LoadTexture("resources/monsterBall.png");
    TextureManager::GetInstance()->LoadTexture("resources/checkerBoard.png");

    // ===== Sprite =====
    sprites_.resize(1);
    sprites_[0].Initialize(spriteCommon_, "resources/uvChecker.png");
    sprites_[0].SetPosition(spritePos_);
    sprites_[0].SetSize({ 640, 360 });

    // ===== Particle group / emitter（ゲーム固有の設定）=====
    ParticleManager::GetInstance()->CreateParticleGroup("test", "resources/circle.png");

    emitter_ = new ParticleEmitter(
        "test",
        { 0.0f, 2.0f, 0.0f },
        0.1f,
        5
    );

    // ===== Sound（ゲーム固有）=====
    // 例：SoundManagerが値型で使えるならメンバにしてここで init/load
    // sound_.Initialize();
    // sound_.Load("alarm", "resources/Alarm01.mp3");
}

void MyGame::Update() {
    // ★汎用Update（ProcessMessage, input_->Update）
    Framework::Update();
    if (endRequst_) { return; }

    // ----- ImGui -----
    imguiManager_->Begin();
    imguiManager_->ShowSpriteController(spritePos_);
    imguiManager_->End();
    sprites_[0].SetPosition(spritePos_);

    // ----- ゲーム更新 -----
    float deltaTime = dxCommon_->GetDeltaTime();

    camera_->Update();
    object3d_->Update();

    if (emitter_) {
        emitter_->Update(deltaTime);
    }

    ParticleManager::GetInstance()->Update(
        camera_->GetViewMatrix(),
        camera_->GetProjectionMatrix()
    );

    for (auto& s : sprites_) { s.Update(); }

    // ----- 例：SPACEでサウンド -----
    if (input_->TriggerKey(DIK_SPACE)) {
        // sound_.Play("alarm");
    }
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

void MyGame::Finalize() {
    // ゲーム固有の後始末（先に消す）
    // sound_.Finalize();

    delete emitter_;
    emitter_ = nullptr;

    delete object3d_;
    object3d_ = nullptr;

    delete camera_;
    camera_ = nullptr;

    delete object3dCommon_;
    object3dCommon_ = nullptr;

    ModelManager::GetInstance()->Finalize();

    // ★最後に汎用Finalize
    Framework::Finalize();
}
