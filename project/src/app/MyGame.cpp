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


MyGame::MyGame() = default;
MyGame::~MyGame() = default;


void MyGame::Initialize() {
    Framework::Initialize();

    object3dCommon_ = std::make_unique<Object3dCommon>();
    object3dCommon_->Initialize(dxCommon_.get(), srvManager_.get());

    camera_ = std::make_unique<Camera>();
    camera_->SetRotate({ 0.3f, 0.0f, 0.0f });
    camera_->SetTranslate({ 0.0f, 4.0f, -10.0f });
    object3dCommon_->SetDefaultCamera(camera_.get()); // 借りるだけなので get()

    ModelManager::GetInstance()->Initialize(dxCommon_.get(), srvManager_.get());
    ModelManager::GetInstance()->LoadModel("plane.obj");

    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCommon_.get());
    object3d_->SetModel("plane.obj");

    TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
    TextureManager::GetInstance()->LoadTexture("resources/monsterBall.png");
    TextureManager::GetInstance()->LoadTexture("resources/checkerBoard.png");

    sprites_.resize(1);
    sprites_[0].Initialize(spriteCommon_.get(), "resources/uvChecker.png");
    sprites_[0].SetPosition(spritePos_);
    sprites_[0].SetSize({ 640, 360 });

    ParticleManager::GetInstance()->CreateParticleGroup("test", "resources/circle.png");

    emitter_ = std::make_unique<ParticleEmitter>(
        "test",
        Math::Vector3{ 0.0f, 2.0f, 0.0f },
        0.1f,
        5
    );
}

void MyGame::Update() {
    Framework::Update();
    if (endRequst_) { return; }

    imguiManager_->Begin();
    imguiManager_->ShowSpriteController(spritePos_);
    imguiManager_->End();
    sprites_[0].SetPosition(spritePos_);

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
    // unique_ptrなので delete 不要（必要なら明示的解放は reset）
    emitter_.reset();
    object3d_.reset();
    camera_.reset();
    object3dCommon_.reset();

    ModelManager::GetInstance()->Finalize();
    Framework::Finalize();
}