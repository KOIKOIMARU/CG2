#include "engine/scene/BonusShowcaseScene.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>

#include "engine/3d/Camera.h"
#include "engine/3d/ModelManager.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/3d/Skybox.h"
#include "engine/base/DirectXCommon.h"
#include "engine/io/Input.h"
#include "engine/scene/SceneManager.h"
#include "engine/scene/SceneType.h"

namespace {

constexpr const char* kEnvironmentTexturePath =
    "resources/skybox/kloofendal_48d_partly_cloudy_puresky_4k_cube.dds";
constexpr Math::Vector3 kBoneDebugOffset = { 1.75f, 0.0f, 0.0f };

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

BonusShowcaseScene::BonusShowcaseScene() = default;
BonusShowcaseScene::~BonusShowcaseScene() = default;

void BonusShowcaseScene::Initialize()
{
    object3dCommon_ = std::make_unique<Object3dCommon>();
    object3dCommon_->Initialize(dxCommon_, srvManager_);
    object3dCommon_->SetEnvironmentTexturePath(kEnvironmentTexturePath);

    camera_ = std::make_unique<Camera>();
    camera_->SetRotate({ 0.08f, 0.0f, 0.0f });
    camera_->SetTranslate({ 0.0f, 2.2f, -13.5f });
    camera_->SetFovY(0.52f);
    object3dCommon_->SetDefaultCamera(camera_.get());

    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(
        dxCommon_,
        srvManager_,
        kEnvironmentTexturePath);

    ModelManager::GetInstance()->Initialize(dxCommon_, srvManager_);
    ModelManager::GetInstance()->SetEnvironmentTexturePath(
        kEnvironmentTexturePath);
    ModelManager::GetInstance()->CreateSphere(
        "bonus_joint_sphere",
        10,
        18,
        1.0f,
        "resources/human/white.png");
    ModelManager::GetInstance()->CreateBox(
        "bonus_bone_box",
        1.0f,
        1.0f,
        1.0f,
        "resources/human/white.png");
    ModelManager::GetInstance()->CreateBox(
        "bonus_weapon_box",
        1.0f,
        1.0f,
        1.0f,
        "resources/human/white.png");
    ModelManager::GetInstance()->CreateSphere(
        "bonus_hand_particle",
        8,
        12,
        1.0f,
        "resources/human/white.png");
    ModelManager::GetInstance()->LoadModel("simpleSkin/simpleSkin.gltf");
    ModelManager::GetInstance()->LoadModel("human/walk.gltf");
    ModelManager::GetInstance()->LoadModel("multiMesh.obj");
    ModelManager::GetInstance()->LoadModel("multiMaterial.obj");

    simpleSkinObject_ = CreateDisplayObject(
        "simpleSkin/simpleSkin.gltf",
        { -4.45f, 0.0f, 0.0f },
        { 0.82f, 0.82f, 0.82f });
    simpleSkinObject_->SetLightingMode(1);
    simpleSkinObject_->SetEnvironmentCoefficient(0.0f);

    humanWalkObject_ = CreateDisplayObject(
        "human/walk.gltf",
        { -2.25f, 0.0f, 0.0f },
        { 1.05f, 1.05f, 1.05f });

    if (humanWalkObject_->HasSkeleton()) {
        const Skeleton& skeleton = humanWalkObject_->GetSkeleton();
        const auto rightHand = skeleton.jointMap.find("mixamorig:RightHand");
        const auto rightForeArm =
            skeleton.jointMap.find("mixamorig:RightForeArm");
        const auto leftHand = skeleton.jointMap.find("mixamorig:LeftHand");
        if (rightHand != skeleton.jointMap.end()) {
            rightHandJointIndex_ = rightHand->second;
        }
        if (rightForeArm != skeleton.jointMap.end()) {
            rightForeArmJointIndex_ = rightForeArm->second;
        }
        if (leftHand != skeleton.jointMap.end()) {
            leftHandJointIndex_ = leftHand->second;
        }
    }

    weaponBladeObject_ = CreateDisplayObject(
        "bonus_weapon_box",
        { 0.0f, -100.0f, 0.0f },
        { 0.055f, 0.12f, 0.90f });
    weaponBladeObject_->SetLightingMode(0);
    weaponBladeObject_->SetEnvironmentCoefficient(0.0f);
    weaponBladeObject_->SetColor({ 0.18f, 0.95f, 1.0f, 1.0f });

    weaponGuardObject_ = CreateDisplayObject(
        "bonus_weapon_box",
        { 0.0f, -100.0f, 0.0f },
        { 0.38f, 0.07f, 0.07f });
    weaponGuardObject_->SetLightingMode(1);
    weaponGuardObject_->SetEnvironmentCoefficient(0.22f);
    weaponGuardObject_->SetColor({ 1.0f, 0.70f, 0.18f, 1.0f });

    weaponGripObject_ = CreateDisplayObject(
        "bonus_weapon_box",
        { 0.0f, -100.0f, 0.0f },
        { 0.09f, 0.09f, 0.28f });
    weaponGripObject_->SetLightingMode(1);
    weaponGripObject_->SetEnvironmentCoefficient(0.08f);
    weaponGripObject_->SetColor({ 0.10f, 0.12f, 0.18f, 1.0f });

    InitializeHandParticles();

    multiMeshObject_ = CreateDisplayObject(
        "multiMesh.obj",
        { 2.05f, 0.55f, 0.0f },
        { 0.72f, 0.72f, 0.72f });
    multiMeshObject_->SetRotate({ 0.0f, 0.35f, 0.0f });

    multiMaterialObject_ = CreateDisplayObject(
        "multiMaterial.obj",
        { 4.45f, 0.55f, 0.0f },
        { 0.72f, 0.72f, 0.72f });
    multiMaterialObject_->SetRotate({ 0.0f, -0.35f, 0.0f });

    InitializeBoneDebug();
    camera_->Update();
}

void BonusShowcaseScene::Finalize()
{
    jointDebugObjects_.clear();
    boneDebugObjects_.clear();
    for (HandParticle& particle : handParticles_) {
        particle.object.reset();
        particle.active = false;
    }
    handEmitterCoreObject_.reset();
    weaponGripObject_.reset();
    weaponGuardObject_.reset();
    weaponBladeObject_.reset();
    multiMaterialObject_.reset();
    multiMeshObject_.reset();
    humanWalkObject_.reset();
    simpleSkinObject_.reset();
    skybox_.reset();
    camera_.reset();
    object3dCommon_.reset();
}

void BonusShowcaseScene::Update()
{
    if (input_ &&
        (input_->TriggerKey(DIK_F2) || input_->TriggerKey(DIK_ESCAPE))) {
        if (sceneManager_) {
            sceneManager_->SetNextScene(SceneType::Title);
        }
        return;
    }
    if (input_ && input_->TriggerKey(DIK_B)) {
        showBoneDebug_ = !showBoneDebug_;
    }

    const float deltaTime = dxCommon_ ? dxCommon_->GetDeltaTime() : 0.0f;
    demonstrationTime_ += deltaTime;

    camera_->Update();
    skybox_->Update(camera_.get());
    simpleSkinObject_->UpdateAnimation(deltaTime);
    humanWalkObject_->UpdateAnimation(deltaTime);

    const float turn = demonstrationTime_ * 0.35f;
    multiMeshObject_->SetRotate({ 0.0f, 0.35f + turn, 0.0f });
    multiMaterialObject_->SetRotate({ 0.0f, -0.35f - turn, 0.0f });

    simpleSkinObject_->Update();
    humanWalkObject_->Update();
    multiMeshObject_->Update();
    multiMaterialObject_->Update();
    UpdateHeldWeapon();
    UpdateHandParticles(deltaTime);
    UpdateBoneDebug();
    DrawShowcaseHud();
}

void BonusShowcaseScene::Draw()
{
    skybox_->Draw();
    object3dCommon_->CommonDrawSetting();
    simpleSkinObject_->Draw();
    humanWalkObject_->Draw();
    weaponGripObject_->Draw();
    weaponGuardObject_->Draw();
    weaponBladeObject_->Draw();
    handEmitterCoreObject_->Draw();
    for (HandParticle& particle : handParticles_) {
        if (particle.active) {
            particle.object->Draw();
        }
    }
    multiMeshObject_->Draw();
    multiMaterialObject_->Draw();

    if (!showBoneDebug_) {
        return;
    }

    for (auto& bone : boneDebugObjects_) {
        bone->Draw();
    }
    for (auto& joint : jointDebugObjects_) {
        joint->Draw();
    }
}

std::unique_ptr<Object3d> BonusShowcaseScene::CreateDisplayObject(
    const char* modelName,
    const Math::Vector3& translate,
    const Math::Vector3& scale)
{
    auto object = std::make_unique<Object3d>();
    object->Initialize(object3dCommon_.get());
    object->SetModel(modelName);
    object->SetTranslate(translate);
    object->SetScale(scale);
    object->SetLightingMode(2);
    object->SetEnvironmentCoefficient(0.12f);
    object->SetShadowReceiveStrength(0.0f);
    object->Update();
    return object;
}

void BonusShowcaseScene::InitializeBoneDebug()
{
    jointDebugObjects_.clear();
    boneDebugObjects_.clear();
    if (!humanWalkObject_ || !humanWalkObject_->HasSkeleton()) {
        return;
    }

    const Skeleton& skeleton = humanWalkObject_->GetSkeleton();
    jointDebugObjects_.reserve(skeleton.joints.size());
    boneDebugObjects_.reserve(skeleton.joints.size());
    for (const Joint& joint : skeleton.joints) {
        auto jointObject = CreateDisplayObject(
            "bonus_joint_sphere",
            { 0.0f, -100.0f, 0.0f },
            { 0.020f, 0.020f, 0.020f });
        jointObject->SetLightingMode(0);
        jointObject->SetEnvironmentCoefficient(0.0f);
        jointObject->SetColor({ 0.28f, 0.95f, 1.0f, 1.0f });
        jointDebugObjects_.push_back(std::move(jointObject));

        if (joint.parent.has_value()) {
            auto boneObject = CreateDisplayObject(
            "bonus_bone_box",
            { 0.0f, -100.0f, 0.0f },
            { 0.012f, 0.012f, 0.012f });
            boneObject->SetLightingMode(0);
            boneObject->SetEnvironmentCoefficient(0.0f);
            boneObject->SetColor({ 1.0f, 0.72f, 0.18f, 1.0f });
            boneDebugObjects_.push_back(std::move(boneObject));
        }
    }
}

void BonusShowcaseScene::UpdateBoneDebug()
{
    if (!humanWalkObject_ || !humanWalkObject_->HasSkeleton()) {
        return;
    }

    const Skeleton& skeleton = humanWalkObject_->GetSkeleton();
    const Math::Matrix4x4& worldMatrix = humanWalkObject_->GetWorldMatrix();
    for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex) {
        const Math::Vector3 position = ExtractTranslation(Math::Multiply(
            skeleton.joints[jointIndex].skeletonSpaceMatrix,
            worldMatrix));
        jointDebugObjects_[jointIndex]->SetTranslate({
            position.x + kBoneDebugOffset.x,
            position.y + kBoneDebugOffset.y,
            position.z + kBoneDebugOffset.z
        });
        jointDebugObjects_[jointIndex]->Update();
    }

    size_t boneIndex = 0;
    for (const Joint& joint : skeleton.joints) {
        if (!joint.parent.has_value()) {
            continue;
        }

        const Math::Vector3 jointPosition = ExtractTranslation(Math::Multiply(
            joint.skeletonSpaceMatrix,
            worldMatrix));
        const Math::Vector3 parentPosition = ExtractTranslation(Math::Multiply(
            skeleton.joints[*joint.parent].skeletonSpaceMatrix,
            worldMatrix));
        const Math::Vector3 difference{
            jointPosition.x - parentPosition.x,
            jointPosition.y - parentPosition.y,
            jointPosition.z - parentPosition.z
        };
        const float length = std::sqrt(
            difference.x * difference.x +
            difference.y * difference.y +
            difference.z * difference.z);
        const float safeLength = (std::max)(length, 0.001f);
        const Math::Vector3 direction{
            difference.x / safeLength,
            difference.y / safeLength,
            difference.z / safeLength
        };
        const Math::Vector3 center{
            (jointPosition.x + parentPosition.x) * 0.5f + kBoneDebugOffset.x,
            (jointPosition.y + parentPosition.y) * 0.5f + kBoneDebugOffset.y,
            (jointPosition.z + parentPosition.z) * 0.5f + kBoneDebugOffset.z
        };

        Object3d* boneObject = boneDebugObjects_[boneIndex].get();
        boneObject->SetTranslate(center);
        boneObject->SetRotate(MakeBoneRotate(direction));
        boneObject->SetScale({ 0.012f, 0.012f, safeLength });
        boneObject->Update();
        ++boneIndex;
    }
}

bool BonusShowcaseScene::TryGetJointWorldPosition(
    int32_t jointIndex,
    Math::Vector3& position) const
{
    if (!humanWalkObject_ || !humanWalkObject_->HasSkeleton() ||
        jointIndex < 0) {
        return false;
    }

    const Skeleton& skeleton = humanWalkObject_->GetSkeleton();
    if (static_cast<size_t>(jointIndex) >= skeleton.joints.size()) {
        return false;
    }

    position = ExtractTranslation(Math::Multiply(
        skeleton.joints[static_cast<size_t>(jointIndex)].skeletonSpaceMatrix,
        humanWalkObject_->GetWorldMatrix()));
    return true;
}

void BonusShowcaseScene::UpdateHeldWeapon()
{
    Math::Vector3 handPosition{};
    Math::Vector3 foreArmPosition{};
    if (!TryGetJointWorldPosition(rightHandJointIndex_, handPosition) ||
        !TryGetJointWorldPosition(rightForeArmJointIndex_, foreArmPosition)) {
        return;
    }

    const Math::Vector3 armDirection{
        handPosition.x - foreArmPosition.x,
        handPosition.y - foreArmPosition.y,
        handPosition.z - foreArmPosition.z
    };
    const float armLength = std::sqrt(
        armDirection.x * armDirection.x +
        armDirection.y * armDirection.y +
        armDirection.z * armDirection.z);
    if (armLength <= 0.0001f) {
        return;
    }

    const Math::Vector3 direction{
        armDirection.x / armLength,
        armDirection.y / armLength,
        armDirection.z / armLength
    };
    const Math::Vector3 rotate = MakeBoneRotate(direction);
    const auto pointAlongWeapon = [&](float distance) {
        return Math::Vector3{
            handPosition.x + direction.x * distance,
            handPosition.y + direction.y * distance,
            handPosition.z + direction.z * distance
        };
    };

    weaponGripObject_->SetTranslate(pointAlongWeapon(0.08f));
    weaponGripObject_->SetRotate(rotate);
    weaponGripObject_->Update();

    weaponGuardObject_->SetTranslate(pointAlongWeapon(0.24f));
    weaponGuardObject_->SetRotate(rotate);
    weaponGuardObject_->Update();

    weaponBladeObject_->SetTranslate(pointAlongWeapon(0.72f));
    weaponBladeObject_->SetRotate(rotate);
    weaponBladeObject_->Update();
}

void BonusShowcaseScene::InitializeHandParticles()
{
    handParticleSerial_ = 0;
    handParticleSpawnAccumulator_ = 0.0f;
    handEmitterCoreObject_ = CreateDisplayObject(
        "bonus_hand_particle",
        { 0.0f, -100.0f, 0.0f },
        { 0.065f, 0.065f, 0.065f });
    handEmitterCoreObject_->SetLightingMode(0);
    handEmitterCoreObject_->SetEnvironmentCoefficient(0.0f);
    handEmitterCoreObject_->SetColor({ 0.26f, 0.92f, 1.0f, 1.0f });

    for (HandParticle& particle : handParticles_) {
        particle.object = CreateDisplayObject(
            "bonus_hand_particle",
            { 0.0f, -100.0f, 0.0f },
            { 0.025f, 0.025f, 0.025f });
        particle.object->SetLightingMode(0);
        particle.object->SetEnvironmentCoefficient(0.0f);
        particle.object->SetColor({ 0.20f, 0.92f, 1.0f, 1.0f });
        particle.active = false;
    }
}

void BonusShowcaseScene::UpdateHandParticles(float deltaTime)
{
    Math::Vector3 emitterPosition{};
    if (!TryGetJointWorldPosition(leftHandJointIndex_, emitterPosition)) {
        return;
    }

    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 0.1f);
    const float corePulse =
        0.060f + 0.012f * std::sin(demonstrationTime_ * 8.0f);
    handEmitterCoreObject_->SetTranslate(emitterPosition);
    handEmitterCoreObject_->SetScale({ corePulse, corePulse, corePulse });
    handEmitterCoreObject_->Update();

    handParticleSpawnAccumulator_ += safeDeltaTime;
    constexpr float kSpawnInterval = 0.03f;
    while (handParticleSpawnAccumulator_ >= kSpawnInterval) {
        handParticleSpawnAccumulator_ -= kSpawnInterval;
        SpawnHandParticle(emitterPosition);
    }

    for (HandParticle& particle : handParticles_) {
        if (!particle.active) {
            continue;
        }

        particle.age += safeDeltaTime;
        if (particle.age >= particle.lifetime) {
            particle.active = false;
            particle.object->SetTranslate({ 0.0f, -100.0f, 0.0f });
            particle.object->Update();
            continue;
        }

        Math::Vector3 position = particle.object->GetTranslate();
        position.x += particle.velocity.x * safeDeltaTime;
        position.y += particle.velocity.y * safeDeltaTime;
        position.z += particle.velocity.z * safeDeltaTime;
        particle.velocity.y += 0.16f * safeDeltaTime;

        const float lifeRate = std::clamp(
            particle.age / particle.lifetime,
            0.0f,
            1.0f);
        const float pulse =
            0.85f + 0.15f * std::sin(particle.age * 28.0f);
        const float scale =
            (0.058f * (1.0f - lifeRate) + 0.012f) * pulse;
        particle.object->SetTranslate(position);
        particle.object->SetScale({ scale, scale, scale });
        particle.object->SetRotate({
            demonstrationTime_ * 1.4f,
            demonstrationTime_ * 1.8f,
            0.0f
        });
        particle.object->Update();
    }
}

void BonusShowcaseScene::SpawnHandParticle(
    const Math::Vector3& emitterPosition)
{
    HandParticle* availableParticle = nullptr;
    for (HandParticle& particle : handParticles_) {
        if (!particle.active) {
            availableParticle = &particle;
            break;
        }
    }
    if (!availableParticle) {
        return;
    }

    const uint32_t serial = handParticleSerial_++;
    const float phase = static_cast<float>(serial) * 2.39996323f;
    const float variation = static_cast<float>(serial % 5) / 4.0f;
    const float radius = 0.018f + variation * 0.018f;

    availableParticle->active = true;
    availableParticle->age = 0.0f;
    availableParticle->lifetime = 0.78f + variation * 0.20f;
    availableParticle->velocity = {
        std::cos(phase) * (0.18f + variation * 0.10f),
        0.20f + variation * 0.14f,
        std::sin(phase) * (0.15f + variation * 0.09f)
    };
    availableParticle->object->SetTranslate({
        emitterPosition.x + std::cos(phase) * radius,
        emitterPosition.y + 0.02f,
        emitterPosition.z + std::sin(phase) * radius
    });
    availableParticle->object->SetScale({ 0.060f, 0.060f, 0.060f });

    switch (serial % 3) {
    case 0:
        availableParticle->object->SetColor({ 0.18f, 0.92f, 1.0f, 1.0f });
        break;
    case 1:
        availableParticle->object->SetColor({ 0.72f, 0.36f, 1.0f, 1.0f });
        break;
    default:
        availableParticle->object->SetColor({ 1.0f, 0.38f, 0.82f, 1.0f });
        break;
    }
    availableParticle->object->Update();
}

void BonusShowcaseScene::DrawShowcaseHud()
{
    ImGui::SetNextWindowPos(ImVec2(24.0f, 24.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(430.0f, 260.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.90f);
    ImGui::Begin(
        "Bonus Feature Showcase",
        nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    ImGui::TextColored(
        ImVec4(0.35f, 0.92f, 1.0f, 1.0f),
        "BONUS SHOWCASE / 70 POINTS");
    ImGui::Separator();
    ImGui::BulletText("Skinning model display (20)");
    ImGui::BulletText("Compute Shader skinning (10)");
    ImGui::BulletText("MultiMesh + MultiMaterial (5)");
    ImGui::BulletText("Animation interpolation: Lerp / Slerp (5)");
    ImGui::BulletText("Animated bone debug display (10)");
    ImGui::BulletText("Weapon attached to animated hand joint (10)");
    ImGui::BulletText("Hand particle: fixed CPU object pool (10)");
    ImGui::Separator();
    ImGui::Text("B: bone display %s", showBoneDebug_ ? "ON" : "OFF");
    ImGui::TextUnformatted("F2 / Esc: return to title");
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(24.0f, 630.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.78f);
    ImGui::Begin(
        "Bonus Labels",
        nullptr,
        ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoInputs);
    ImGui::TextUnformatted(
        "SKIN+COMPUTE    ANIM MODEL    BONE DEBUG    MULTI MESH    MULTI MATERIAL");
    ImGui::TextUnformatted(
        "HAND-JOINT WEAPON    LEFT-HAND PARTICLE POOL");
    ImGui::End();
}
