#include "engine/scene/GamePlayScene.h"

#include "engine/3d/Object3dCommon.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Camera.h"
#include "engine/2d/Sprite.h"
#include "engine/2d/SpriteCommon.h"
#include "engine/3d/TextureManager.h"
#include "engine/3d/ParticleManager.h"
#include "engine/3d/ParticleEmitter.h"
#include "engine/3d/Skybox.h"
#include "engine/base/ImGuiManager.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/SrvManager.h"
#include "engine/3d/ModelManager.h"

GamePlayScene::GamePlayScene() = default;
GamePlayScene::~GamePlayScene() = default;


void GamePlayScene::Initialize() {
    object3dCommon_ = std::make_unique<Object3dCommon>();
    object3dCommon_->Initialize(dxCommon_, srvManager_);

    camera_ = std::make_unique<Camera>();
    camera_->SetRotate({ 0.3f, 0.0f, 0.0f });
    camera_->SetTranslate({ 0.0f, 4.0f, -10.0f });
    object3dCommon_->SetDefaultCamera(camera_.get());

    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(
        dxCommon_,
        srvManager_,
        "resources/skybox/rostock_laage_airport_4k.dds"
    );

    ModelManager::GetInstance()->Initialize(dxCommon_, srvManager_);
    ModelManager::GetInstance()->LoadModel("plane.gltf");

    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCommon_.get());
    object3d_->SetModel("plane.gltf");

    TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
    TextureManager::GetInstance()->LoadTexture("resources/monsterBall.png");
    TextureManager::GetInstance()->LoadTexture("resources/checkerBoard.png");

    ParticleManager::GetInstance()->CreateParticleGroup("test", "resources/circle.png");

    emitter_ = std::make_unique<ParticleEmitter>(
        "test",
        Math::Vector3{ 0.0f, 2.0f, 0.0f },
        0.1f,
        5
    );
}

void GamePlayScene::Update() {
    imguiManager_->ShowSpriteController(spritePos_);
    imguiManager_->ShowGamePlayController(
        objectRotate_,
        lightDirection_,
        lightIntensity_,
        blendModeIndex_,
        pointLightPosition_,
        pointLightIntensity_,
        spotLightPosition_,
        spotLightDirection_,
        spotLightIntensity_
    );

    for (auto& sprite : sprites_) {
        sprite.SetPosition(spritePos_);
    }
    object3d_->SetRotate(objectRotate_);
    object3d_->SetDirectionalLightDirection(lightDirection_);
    object3d_->SetDirectionalLightIntensity(lightIntensity_);
    object3d_->SetPointLightPosition(pointLightPosition_);
    object3d_->SetPointLightIntensity(pointLightIntensity_);
    object3d_->SetSpotLightPosition(spotLightPosition_);
    object3d_->SetSpotLightDirection(spotLightDirection_);
    object3d_->SetSpotLightIntensity(spotLightIntensity_);
    object3dCommon_->SetBlendMode(
        static_cast<BlendMode>(blendModeIndex_));

    float deltaTime = dxCommon_->GetDeltaTime();

    camera_->Update();
    skybox_->Update(camera_.get());
    object3d_->Update();

    if (emitter_) {
        emitter_->Update(deltaTime);
    }

    ParticleManager::GetInstance()->Update(
        camera_->GetViewMatrix(),
        camera_->GetProjectionMatrix()
    );

    for (auto& s : sprites_) {
        s.Update();
    }
}

void GamePlayScene::Draw() {
    skybox_->Draw();

    ParticleManager::GetInstance()->Draw();

    object3dCommon_->CommonDrawSetting();
    object3d_->Draw();

    spriteCommon_->CommonDrawSetting();
    for (auto& s : sprites_) {
        s.Draw();
    }
}

void GamePlayScene::Finalize() {
    emitter_.reset();
    object3d_.reset();
    skybox_.reset();
    camera_.reset();
    object3dCommon_.reset();

    ModelManager::GetInstance()->Finalize();
}
