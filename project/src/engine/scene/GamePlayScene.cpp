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
#include "engine/io/Input.h"
#include <algorithm>

namespace {

Math::Vector3 Add(const Math::Vector3& a, const Math::Vector3& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

Math::Vector3 Subtract(const Math::Vector3& a, const Math::Vector3& b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

Math::Vector3 MakeForwardVector(const Math::Vector3& rotate)
{
    const float cosPitch = std::cos(rotate.x);
    const float sinPitch = std::sin(rotate.x);
    const float cosYaw = std::cos(rotate.y);
    const float sinYaw = std::sin(rotate.y);

    return Math::Normalize({
        sinYaw * cosPitch,
        -sinPitch,
        cosYaw * cosPitch
    });
}

Math::Vector3 MakeRightVector(const Math::Vector3& rotate)
{
    const float yaw = rotate.y + (3.14159265358979323846f * 0.5f);
    return Math::Normalize({
        std::sin(yaw),
        0.0f,
        std::cos(yaw)
    });
}

} // namespace

GamePlayScene::GamePlayScene() = default;
GamePlayScene::~GamePlayScene() = default;


void GamePlayScene::Initialize() {
    const std::string environmentTexturePath =
        "resources/skybox/kloofendal_48d_partly_cloudy_puresky_4k_cube.dds";

    object3dCommon_ = std::make_unique<Object3dCommon>();
    object3dCommon_->Initialize(dxCommon_, srvManager_);
    object3dCommon_->SetEnvironmentTexturePath(environmentTexturePath);

    camera_ = std::make_unique<Camera>();
    camera_->SetRotate({ 0.3f, 0.0f, 0.0f });
    camera_->SetTranslate({ 0.0f, 4.0f, -10.0f });
    object3dCommon_->SetDefaultCamera(camera_.get());

    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(
        dxCommon_,
        srvManager_,
        environmentTexturePath
    );

    ModelManager::GetInstance()->Initialize(dxCommon_, srvManager_);
    ModelManager::GetInstance()->SetEnvironmentTexturePath(
        environmentTexturePath
    );
    ModelManager::GetInstance()->CreatePlane(
        "primitive_plane",
        8.0f,
        8.0f,
        "resources/checkerBoard.png"
    );
    ModelManager::GetInstance()->CreateRing(
        "primitive_ring",
        32,
        2.0f,
        1.0f,
        "resources/gradationLine.png"
    );
    ModelManager::GetInstance()->LoadModel("sphere.obj");

    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCommon_.get());
    object3d_->SetModel("primitive_plane");
    object3d_->SetEnvironmentCoefficient(environmentCoefficient_);
    object3d_->SetRotate({ 1.5707963f, 0.0f, 0.0f });

    ringObject_ = std::make_unique<Object3d>();
    ringObject_->Initialize(object3dCommon_.get());
    ringObject_->SetModel("primitive_ring");
    ringObject_->SetTranslate({ 0.0f, 0.05f, 0.0f });
    ringObject_->SetRotate({ 1.5707963f, 0.0f, 0.0f });
    ringObject_->SetEnvironmentCoefficient(0.0f);

    sphereObject_ = std::make_unique<Object3d>();
    sphereObject_->Initialize(object3dCommon_.get());
    sphereObject_->SetModel("sphere.obj");
    sphereObject_->SetScale({ 1.5f, 1.5f, 1.5f });
    sphereObject_->SetTranslate({ 0.0f, 1.5f, 0.0f });
    sphereObject_->SetEnvironmentCoefficient(environmentCoefficient_);

    TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
    TextureManager::GetInstance()->LoadTexture("resources/monsterBall.png");
    TextureManager::GetInstance()->LoadTexture("resources/checkerBoard.png");
    TextureManager::GetInstance()->LoadTexture("resources/gradationLine.png");
    TextureManager::GetInstance()->LoadTexture("resources/circle2.png");

    ParticleManager::GetInstance()->CreateParticleGroup(
        "test",
        "resources/circle2.png"
    );

    emitter_ = std::make_unique<ParticleEmitter>(
        "test",
        Math::Vector3{ 0.0f, 2.0f, 0.0f },
        0.15f,
        8
    );
}

void GamePlayScene::Update() {
    const float deltaTime = dxCommon_->GetDeltaTime();

    if (input_ && input_->TriggerKey(DIK_F1)) {
        isDebugCameraEnabled_ = !isDebugCameraEnabled_;
    }

    if (isDebugCameraEnabled_) {
        UpdateDebugCamera(deltaTime);
    }

    imguiManager_->ShowSpriteController(spritePos_);
    imguiManager_->ShowGamePlayController(
        objectRotate_,
        lightDirection_,
        lightIntensity_,
        blendModeIndex_,
        environmentCoefficient_,
        showPlane_,
        showRing_,
        showSphere_,
        showParticle_,
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
    object3d_->SetEnvironmentCoefficient(environmentCoefficient_);
    object3d_->SetPointLightPosition(pointLightPosition_);
    object3d_->SetPointLightIntensity(pointLightIntensity_);
    object3d_->SetSpotLightPosition(spotLightPosition_);
    object3d_->SetSpotLightDirection(spotLightDirection_);
    object3d_->SetSpotLightIntensity(spotLightIntensity_);
    ringObject_->SetRotate({ 1.5707963f, objectRotate_.y, objectRotate_.z });
    ringObject_->SetDirectionalLightDirection(lightDirection_);
    ringObject_->SetDirectionalLightIntensity(lightIntensity_);
    ringObject_->SetPointLightPosition(pointLightPosition_);
    ringObject_->SetPointLightIntensity(pointLightIntensity_);
    ringObject_->SetSpotLightPosition(spotLightPosition_);
    ringObject_->SetSpotLightDirection(spotLightDirection_);
    ringObject_->SetSpotLightIntensity(spotLightIntensity_);
    sphereObject_->SetRotate(objectRotate_);
    sphereObject_->SetDirectionalLightDirection(lightDirection_);
    sphereObject_->SetDirectionalLightIntensity(lightIntensity_);
    sphereObject_->SetEnvironmentCoefficient(environmentCoefficient_);
    sphereObject_->SetPointLightPosition(pointLightPosition_);
    sphereObject_->SetPointLightIntensity(pointLightIntensity_);
    sphereObject_->SetSpotLightPosition(spotLightPosition_);
    sphereObject_->SetSpotLightDirection(spotLightDirection_);
    sphereObject_->SetSpotLightIntensity(spotLightIntensity_);
    object3dCommon_->SetBlendMode(
        static_cast<BlendMode>(blendModeIndex_));

    camera_->Update();
    skybox_->Update(camera_.get());
    object3d_->Update();
    ringObject_->Update();
    sphereObject_->Update();

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

void GamePlayScene::UpdateDebugCamera(float deltaTime)
{
    if (!input_ || !camera_) {
        return;
    }

    Math::Vector3 rotate = camera_->GetRotate();
    Math::Vector3 translate = camera_->GetTranslate();

    const float rotateStep = debugCameraRotateSpeed_ * deltaTime;
    if (input_->PushKey(DIK_LEFT)) {
        rotate.y -= rotateStep;
    }
    if (input_->PushKey(DIK_RIGHT)) {
        rotate.y += rotateStep;
    }
    if (input_->PushKey(DIK_UP)) {
        rotate.x -= rotateStep;
    }
    if (input_->PushKey(DIK_DOWN)) {
        rotate.x += rotateStep;
    }

    rotate.x = std::clamp(rotate.x, -1.4f, 1.4f);

    const Math::Vector3 forward = MakeForwardVector(rotate);
    const Math::Vector3 right = MakeRightVector(rotate);
    const Math::Vector3 up = { 0.0f, 1.0f, 0.0f };

    Math::Vector3 move = { 0.0f, 0.0f, 0.0f };
    if (input_->PushKey(DIK_W)) {
        move = Add(move, forward);
    }
    if (input_->PushKey(DIK_S)) {
        move = Subtract(move, forward);
    }
    if (input_->PushKey(DIK_D)) {
        move = Add(move, right);
    }
    if (input_->PushKey(DIK_A)) {
        move = Subtract(move, right);
    }
    if (input_->PushKey(DIK_SPACE)) {
        move = Add(move, up);
    }
    if (input_->PushKey(DIK_LSHIFT) || input_->PushKey(DIK_RSHIFT)) {
        move = Subtract(move, up);
    }

    if (move.x != 0.0f || move.y != 0.0f || move.z != 0.0f) {
        move = Math::Normalize(move) * (debugCameraMoveSpeed_ * deltaTime);
        translate = Add(translate, move);
    }

    camera_->SetRotate(rotate);
    camera_->SetTranslate(translate);
}

void GamePlayScene::Draw() {
    skybox_->Draw();

    object3dCommon_->CommonDrawSetting();
    if (showPlane_) {
        object3d_->Draw();
    }
    if (showRing_) {
        ringObject_->Draw();
    }
    if (showSphere_) {
        sphereObject_->Draw();
    }

    if (showParticle_) {
        ParticleManager::GetInstance()->Draw();
    }

    spriteCommon_->CommonDrawSetting();
    for (auto& s : sprites_) {
        s.Draw();
    }
}

void GamePlayScene::Finalize() {
    emitter_.reset();
    ringObject_.reset();
    sphereObject_.reset();
    object3d_.reset();
    skybox_.reset();
    camera_.reset();
    object3dCommon_.reset();

    ModelManager::GetInstance()->Finalize();
}
