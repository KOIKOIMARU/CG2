#include "engine/scene/GamePlayScene.h"

#include "engine/3d/Object3dCommon.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Model.h"
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

Math::Vector3 ExtractTranslation(const Math::Matrix4x4& matrix)
{
    return {
        matrix.m[3][0],
        matrix.m[3][1],
        matrix.m[3][2]
    };
}

Math::Vector3 MakeBoneRotate(const Math::Vector3& direction)
{
    const float horizontalLength =
        std::sqrt(direction.x * direction.x + direction.z * direction.z);

    return {
        std::atan2(-direction.y, horizontalLength),
        std::atan2(direction.x, direction.z),
        0.0f
    };
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
    camera_->SetRotate({ 0.2f, 0.0f, 0.0f });
    camera_->SetTranslate({ 0.0f, 6.0f, -24.0f });
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
    ModelManager::GetInstance()->CreateTriangle(
        "primitive_triangle",
        1.6f,
        1.6f,
        "resources/uvChecker.png"
    );
    ModelManager::GetInstance()->CreateCircle(
        "primitive_circle",
        32,
        0.9f,
        "resources/uvChecker.png"
    );
    ModelManager::GetInstance()->CreateRing(
        "primitive_ring",
        32,
        2.0f,
        1.0f,
        "resources/gradationLine.png"
    );
    ModelManager::GetInstance()->CreateSphere(
        "primitive_sphere",
        16,
        32,
        1.0f,
        "resources/uvChecker.png"
    );
    ModelManager::GetInstance()->CreateSphere(
        "debug_joint_sphere",
        16,
        32,
        1.0f,
        "resources/human/white.png"
    );
    ModelManager::GetInstance()->CreateTorus(
        "primitive_torus",
        32,
        16,
        0.8f,
        0.3f,
        "resources/uvChecker.png"
    );
    ModelManager::GetInstance()->CreateCylinder(
        "primitive_cylinder",
        32,
        1.2f,
        1.2f,
        2.5f,
        "resources/gradationLine.png"
    );
    ModelManager::GetInstance()->CreateCone(
        "primitive_cone",
        32,
        0.8f,
        1.6f,
        "resources/uvChecker.png"
    );
    ModelManager::GetInstance()->CreateBox(
        "primitive_box",
        1.5f,
        1.5f,
        1.5f,
        "resources/uvChecker.png"
    );
    ModelManager::GetInstance()->CreateBox(
        "debug_bone_box",
        1.0f,
        1.0f,
        1.0f,
        "resources/human/white.png"
    );
    ModelManager::GetInstance()->LoadModel("AnimatedCube/AnimatedCube.gltf");
    ModelManager::GetInstance()->LoadModel("simpleSkin/simpleSkin.gltf");
    ModelManager::GetInstance()->LoadModel("human/sneakWalk.gltf");
    ModelManager::GetInstance()->LoadModel("human/walk.gltf");

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

    cylinderObject_ = std::make_unique<Object3d>();
    cylinderObject_->Initialize(object3dCommon_.get());
    cylinderObject_->SetModel("primitive_cylinder");
    cylinderObject_->SetTranslate({ 0.0f, 0.0f, 0.0f });
    cylinderObject_->SetEnvironmentCoefficient(0.0f);
    cylinderObject_->SetLightingMode(0);
    cylinderObject_->SetColor(cylinderColor_);
    cylinderObject_->SetAlphaReference(cylinderAlphaReference_);

    sphereObject_ = std::make_unique<Object3d>();
    sphereObject_->Initialize(object3dCommon_.get());
    sphereObject_->SetModel("primitive_sphere");
    sphereObject_->SetScale({ 1.0f, 1.0f, 1.0f });
    sphereObject_->SetTranslate({ 0.0f, 1.5f, 3.0f });
    sphereObject_->SetEnvironmentCoefficient(environmentCoefficient_);

    animatedCubeObject_ = std::make_unique<Object3d>();
    animatedCubeObject_->Initialize(object3dCommon_.get());
    animatedCubeObject_->SetModel("AnimatedCube/AnimatedCube.gltf");
    animatedCubeObject_->SetTranslate({ 0.0f, 1.5f, -3.5f });
    animatedCubeObject_->SetScale({ 1.0f, 1.0f, 1.0f });
    animatedCubeObject_->SetEnvironmentCoefficient(0.0f);

    simpleSkinObject_ = std::make_unique<Object3d>();
    simpleSkinObject_->Initialize(object3dCommon_.get());
    simpleSkinObject_->SetModel("simpleSkin/simpleSkin.gltf");
    simpleSkinObject_->SetTranslate({ -7.0f, 0.0f, 0.0f });
    simpleSkinObject_->SetEnvironmentCoefficient(0.0f);

    humanSneakObject_ = std::make_unique<Object3d>();
    humanSneakObject_->Initialize(object3dCommon_.get());
    humanSneakObject_->SetModel("human/sneakWalk.gltf");
    humanSneakObject_->SetTranslate({ 0.0f, 0.0f, 0.0f });
    humanSneakObject_->SetEnvironmentCoefficient(0.0f);

    humanWalkObject_ = std::make_unique<Object3d>();
    humanWalkObject_->Initialize(object3dCommon_.get());
    humanWalkObject_->SetModel("human/walk.gltf");
    humanWalkObject_->SetTranslate({ 7.0f, 0.0f, 0.0f });
    humanWalkObject_->SetEnvironmentCoefficient(0.0f);

    InitializeSkeletonDebugSet(
        simpleSkinDebug_,
        simpleSkinObject_.get(),
        { 1.0f, 1.0f, 1.0f, 1.0f }
    );
    InitializeSkeletonDebugSet(
        humanSneakDebug_,
        humanSneakObject_.get(),
        { 1.0f, 1.0f, 1.0f, 1.0f }
    );
    InitializeSkeletonDebugSet(
        humanWalkDebug_,
        humanWalkObject_.get(),
        { 1.0f, 1.0f, 1.0f, 1.0f }
    );

    const std::pair<const char*, Math::Vector3> primitiveSamples[] = {
        { "primitive_triangle", { -4.5f, 1.0f, 3.0f } },
        { "primitive_circle",   { -2.0f, 1.0f, 3.0f } },
        { "primitive_box",      { 2.5f, 1.0f, 3.0f } },
        { "primitive_torus",    { 5.0f, 1.2f, 3.0f } },
        { "primitive_cone",     { -5.0f, 1.0f, -2.0f } },
    };

    for (const auto& [modelName, position] : primitiveSamples) {
        auto primitiveObject = std::make_unique<Object3d>();
        primitiveObject->Initialize(object3dCommon_.get());
        primitiveObject->SetModel(modelName);
        primitiveObject->SetTranslate(position);
        primitiveObject->SetEnvironmentCoefficient(0.0f);
        primitiveObjects_.push_back(std::move(primitiveObject));
    }

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
        showCylinder_,
        showSphere_,
        showParticle_,
        cylinderColor_,
        cylinderAlphaReference_,
        cylinderUVScrollSpeed_,
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

    cylinderUVOffset_ += cylinderUVScrollSpeed_ * deltaTime;
    Matrix4x4 cylinderUVTransform = Multiply(
        MakeScaleMatrix({ 1.0f, -1.0f, 1.0f }),
        MakeTranslateMatrix({ cylinderUVOffset_, 1.0f, 0.0f })
    );
    cylinderObject_->SetColor(cylinderColor_);
    cylinderObject_->SetAlphaReference(cylinderAlphaReference_);
    cylinderObject_->SetUVTransform(cylinderUVTransform);
    cylinderObject_->SetDirectionalLightDirection(lightDirection_);
    cylinderObject_->SetDirectionalLightIntensity(lightIntensity_);
    cylinderObject_->SetPointLightPosition(pointLightPosition_);
    cylinderObject_->SetPointLightIntensity(pointLightIntensity_);
    cylinderObject_->SetSpotLightPosition(spotLightPosition_);
    cylinderObject_->SetSpotLightDirection(spotLightDirection_);
    cylinderObject_->SetSpotLightIntensity(spotLightIntensity_);

    sphereObject_->SetRotate(objectRotate_);
    sphereObject_->SetDirectionalLightDirection(lightDirection_);
    sphereObject_->SetDirectionalLightIntensity(lightIntensity_);
    sphereObject_->SetEnvironmentCoefficient(environmentCoefficient_);
    sphereObject_->SetPointLightPosition(pointLightPosition_);
    sphereObject_->SetPointLightIntensity(pointLightIntensity_);
    sphereObject_->SetSpotLightPosition(spotLightPosition_);
    sphereObject_->SetSpotLightDirection(spotLightDirection_);
    sphereObject_->SetSpotLightIntensity(spotLightIntensity_);

    if (Model* animatedCubeModel =
            ModelManager::GetInstance()->FindModel("AnimatedCube/AnimatedCube.gltf")) {
        const Animation& animation = animatedCubeModel->GetAnimation();
        if (animation.duration > 0.0f &&
            animatedCubeModel->HasAnimation()) {
            animationTime_ += deltaTime * animation.ticksPerSecond;
            while (animationTime_ > animation.duration) {
                animationTime_ -= animation.duration;
            }

            const std::string& rootNodeName =
                animatedCubeModel->GetRootNode().name;
            auto it = animation.nodeAnimations.find(rootNodeName);
            if (it != animation.nodeAnimations.end()) {
                const QuaternionTransform transform =
                    Model::CalculateValue(it->second, animationTime_);
                animatedCubeObject_->SetScale(transform.scale);
                animatedCubeObject_->SetQuaternionRotate(transform.rotate);
                animatedCubeObject_->SetTranslate({
                    transform.translate.x,
                    transform.translate.y + 1.5f,
                    transform.translate.z - 3.5f
                });
            }
        }
    }

    animatedCubeObject_->SetDirectionalLightDirection(lightDirection_);
    animatedCubeObject_->SetDirectionalLightIntensity(lightIntensity_);
    animatedCubeObject_->SetPointLightPosition(pointLightPosition_);
    animatedCubeObject_->SetPointLightIntensity(pointLightIntensity_);
    animatedCubeObject_->SetSpotLightPosition(spotLightPosition_);
    animatedCubeObject_->SetSpotLightDirection(spotLightDirection_);
    animatedCubeObject_->SetSpotLightIntensity(spotLightIntensity_);

    if (simpleSkinObject_) {
        simpleSkinObject_->UpdateAnimation(deltaTime);
        simpleSkinObject_->SetDirectionalLightDirection(lightDirection_);
        simpleSkinObject_->SetDirectionalLightIntensity(lightIntensity_);
        simpleSkinObject_->SetPointLightPosition(pointLightPosition_);
        simpleSkinObject_->SetPointLightIntensity(pointLightIntensity_);
        simpleSkinObject_->SetSpotLightPosition(spotLightPosition_);
        simpleSkinObject_->SetSpotLightDirection(spotLightDirection_);
        simpleSkinObject_->SetSpotLightIntensity(spotLightIntensity_);
    }

    if (humanSneakObject_) {
        humanSneakObject_->UpdateAnimation(deltaTime);
        humanSneakObject_->SetDirectionalLightDirection(lightDirection_);
        humanSneakObject_->SetDirectionalLightIntensity(lightIntensity_);
        humanSneakObject_->SetPointLightPosition(pointLightPosition_);
        humanSneakObject_->SetPointLightIntensity(pointLightIntensity_);
        humanSneakObject_->SetSpotLightPosition(spotLightPosition_);
        humanSneakObject_->SetSpotLightDirection(spotLightDirection_);
        humanSneakObject_->SetSpotLightIntensity(spotLightIntensity_);
    }

    if (humanWalkObject_) {
        humanWalkObject_->UpdateAnimation(deltaTime);
        humanWalkObject_->SetDirectionalLightDirection(lightDirection_);
        humanWalkObject_->SetDirectionalLightIntensity(lightIntensity_);
        humanWalkObject_->SetPointLightPosition(pointLightPosition_);
        humanWalkObject_->SetPointLightIntensity(pointLightIntensity_);
        humanWalkObject_->SetSpotLightPosition(spotLightPosition_);
        humanWalkObject_->SetSpotLightDirection(spotLightDirection_);
        humanWalkObject_->SetSpotLightIntensity(spotLightIntensity_);
    }

    if (showSkeletonDebug_) {
        UpdateSkeletonDebugSet(simpleSkinDebug_);
        UpdateSkeletonDebugSet(humanSneakDebug_);
        UpdateSkeletonDebugSet(humanWalkDebug_);
    }

    for (auto& primitiveObject : primitiveObjects_) {
        primitiveObject->SetRotate(objectRotate_);
        primitiveObject->SetDirectionalLightDirection(lightDirection_);
        primitiveObject->SetDirectionalLightIntensity(lightIntensity_);
        primitiveObject->SetPointLightPosition(pointLightPosition_);
        primitiveObject->SetPointLightIntensity(pointLightIntensity_);
        primitiveObject->SetSpotLightPosition(spotLightPosition_);
        primitiveObject->SetSpotLightDirection(spotLightDirection_);
        primitiveObject->SetSpotLightIntensity(spotLightIntensity_);
    }

    object3dCommon_->SetBlendMode(
        static_cast<BlendMode>(blendModeIndex_));

    camera_->Update();
    skybox_->Update(camera_.get());
    object3d_->Update();
    ringObject_->Update();
    cylinderObject_->Update();
    sphereObject_->Update();
    animatedCubeObject_->Update();
    if (simpleSkinObject_) {
        simpleSkinObject_->Update();
    }
    if (humanSneakObject_) {
        humanSneakObject_->Update();
    }
    if (humanWalkObject_) {
        humanWalkObject_->Update();
    }
    if (showSkeletonDebug_) {
        for (auto& joint : simpleSkinDebug_.joints) {
            joint->Update();
        }
        for (auto& bone : simpleSkinDebug_.bones) {
            bone->Update();
        }
        for (auto& joint : humanSneakDebug_.joints) {
            joint->Update();
        }
        for (auto& bone : humanSneakDebug_.bones) {
            bone->Update();
        }
        for (auto& joint : humanWalkDebug_.joints) {
            joint->Update();
        }
        for (auto& bone : humanWalkDebug_.bones) {
            bone->Update();
        }
    }
    for (auto& primitiveObject : primitiveObjects_) {
        primitiveObject->Update();
    }

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
    if (showCylinder_) {
        cylinderObject_->Draw();
    }
    if (showSphere_) {
        sphereObject_->Draw();
        animatedCubeObject_->Draw();
        for (auto& primitiveObject : primitiveObjects_) {
            primitiveObject->Draw();
        }
    }

    if (showSkinningSamples_) {
        if (simpleSkinObject_) {
            simpleSkinObject_->Draw();
        }
        if (humanSneakObject_) {
            humanSneakObject_->Draw();
        }
        if (humanWalkObject_) {
            humanWalkObject_->Draw();
        }
    }

    if (showSkinningSamples_ && showSkeletonDebug_) {
        object3dCommon_->SetDepthDrawMode(DepthDrawMode::Overlay);
        object3dCommon_->CommonDrawSetting();
        for (auto& bone : simpleSkinDebug_.bones) {
            bone->Draw();
        }
        for (auto& joint : simpleSkinDebug_.joints) {
            joint->Draw();
        }
        for (auto& bone : humanSneakDebug_.bones) {
            bone->Draw();
        }
        for (auto& joint : humanSneakDebug_.joints) {
            joint->Draw();
        }
        for (auto& bone : humanWalkDebug_.bones) {
            bone->Draw();
        }
        for (auto& joint : humanWalkDebug_.joints) {
            joint->Draw();
        }
        object3dCommon_->SetDepthDrawMode(DepthDrawMode::Normal);
        object3dCommon_->CommonDrawSetting();
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
    cylinderObject_.reset();
    ringObject_.reset();
    sphereObject_.reset();
    animatedCubeObject_.reset();
    simpleSkinObject_.reset();
    humanSneakObject_.reset();
    humanWalkObject_.reset();
    simpleSkinDebug_.joints.clear();
    simpleSkinDebug_.bones.clear();
    humanSneakDebug_.joints.clear();
    humanSneakDebug_.bones.clear();
    humanWalkDebug_.joints.clear();
    humanWalkDebug_.bones.clear();
    primitiveObjects_.clear();
    object3d_.reset();
    skybox_.reset();
    camera_.reset();
    object3dCommon_.reset();

    ModelManager::GetInstance()->Finalize();
}

void GamePlayScene::InitializeSkeletonDebugSet(
    SkeletonDebugSet& debugSet,
    Object3d* source,
    const Math::Vector4& color)
{
    debugSet.source = source;
    debugSet.color = color;
    debugSet.joints.clear();
    debugSet.bones.clear();

    if (!source || !source->HasSkeleton()) {
        return;
    }

    const Skeleton& skeleton = source->GetSkeleton();
    debugSet.joints.reserve(skeleton.joints.size());
    debugSet.bones.reserve(skeleton.joints.size());

    for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex) {
        auto jointObject = std::make_unique<Object3d>();
        jointObject->Initialize(object3dCommon_.get());
        jointObject->SetModel("debug_joint_sphere");
        jointObject->SetScale({ 0.006f, 0.006f, 0.006f });
        jointObject->SetLightingMode(0);
        jointObject->SetEnvironmentCoefficient(0.0f);
        jointObject->SetColor(color);
        debugSet.joints.push_back(std::move(jointObject));

        if (skeleton.joints[jointIndex].parent.has_value()) {
            auto boneObject = std::make_unique<Object3d>();
            boneObject->Initialize(object3dCommon_.get());
            boneObject->SetModel("debug_bone_box");
            boneObject->SetLightingMode(0);
            boneObject->SetEnvironmentCoefficient(0.0f);
            boneObject->SetColor(color);
            debugSet.bones.push_back(std::move(boneObject));
        }
    }
}

void GamePlayScene::UpdateSkeletonDebugSet(SkeletonDebugSet& debugSet)
{
    if (!debugSet.source || !debugSet.source->HasSkeleton()) {
        return;
    }

    const bool isSimpleSkin = debugSet.source == simpleSkinObject_.get();
    const Skeleton& skeleton = debugSet.source->GetSkeleton();
    const Matrix4x4& worldMatrix = debugSet.source->GetWorldMatrix();

    for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex) {
        const Matrix4x4 jointWorldMatrix = Multiply(
            skeleton.joints[jointIndex].skeletonSpaceMatrix,
            worldMatrix
        );
        const Math::Vector3 jointPosition = ExtractTranslation(jointWorldMatrix);

        debugSet.joints[jointIndex]->SetTranslate(jointPosition);
        if (isSimpleSkin) {
            debugSet.joints[jointIndex]->SetScale({ 0.02f, 0.02f, 0.02f });
            if (jointIndex == 0) {
                debugSet.joints[jointIndex]->SetColor({ 1.0f, 0.55f, 0.75f, 1.0f });
            } else {
                debugSet.joints[jointIndex]->SetColor({ 0.55f, 0.9f, 1.0f, 1.0f });
            }
        } else {
            debugSet.joints[jointIndex]->SetScale({ 0.006f, 0.006f, 0.006f });
            debugSet.joints[jointIndex]->SetColor(debugSet.color);
        }
    }

    size_t boneIndex = 0;
    for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex) {
        const auto& joint = skeleton.joints[jointIndex];
        if (!joint.parent.has_value()) {
            continue;
        }

        const Matrix4x4 jointWorldMatrix = Multiply(
            joint.skeletonSpaceMatrix,
            worldMatrix
        );
        const Matrix4x4 parentWorldMatrix = Multiply(
            skeleton.joints[*joint.parent].skeletonSpaceMatrix,
            worldMatrix
        );

        const Math::Vector3 jointPosition = ExtractTranslation(jointWorldMatrix);
        const Math::Vector3 parentPosition = ExtractTranslation(parentWorldMatrix);
        const Math::Vector3 diff = Subtract(jointPosition, parentPosition);
        const float length =
            std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

        if (length <= 0.0001f) {
            if (isSimpleSkin) {
                debugSet.bones[boneIndex]->SetScale({ 0.0015f, 0.0015f, 0.0015f });
            } else {
                debugSet.bones[boneIndex]->SetScale({ 0.002f, 0.002f, 0.002f });
            }
            debugSet.bones[boneIndex]->SetTranslate(parentPosition);
            debugSet.bones[boneIndex]->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
            ++boneIndex;
            continue;
        }

        const Math::Vector3 direction = {
            diff.x / length,
            diff.y / length,
            diff.z / length
        };
        const Math::Vector3 center = {
            (jointPosition.x + parentPosition.x) * 0.5f,
            (jointPosition.y + parentPosition.y) * 0.5f,
            (jointPosition.z + parentPosition.z) * 0.5f
        };

        if (isSimpleSkin) {
            debugSet.bones[boneIndex]->SetScale({ 0.0025f, 0.0025f, length });
        } else {
            debugSet.bones[boneIndex]->SetScale({ 0.006f, 0.006f, length });
        }
        debugSet.bones[boneIndex]->SetRotate(MakeBoneRotate(direction));
        debugSet.bones[boneIndex]->SetTranslate(center);
        debugSet.bones[boneIndex]->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        ++boneIndex;
    }
}
