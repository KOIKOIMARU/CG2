#include "app/GameBonusActor.h"

#include "engine/3d/Model.h"
#include "engine/3d/ModelManager.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Object3dCommon.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

constexpr const char* kSupportActorModelPath = "human/walk.gltf";
constexpr const char* kMultiMeshModelPath = "multiMesh.obj";
constexpr const char* kMultiMaterialModelPath = "multiMaterial.obj";
constexpr float kHiddenObjectY = -1000.0f;

Math::Vector3 ExtractTranslation(const Math::Matrix4x4& matrix)
{
    return {
        matrix.m[3][0],
        matrix.m[3][1],
        matrix.m[3][2],
    };
}

Math::Vector3 MakeBoneRotate(const Math::Vector3& direction)
{
    const float horizontalLength =
        std::sqrt(direction.x * direction.x + direction.z * direction.z);
    return {
        std::atan2(-direction.y, horizontalLength),
        std::atan2(direction.x, direction.z),
        0.0f,
    };
}

} // namespace

bool GameBonusActor::Initialize(Object3dCommon* object3dCommon)
{
    Finalize();
    object3dCommon_ = object3dCommon;
    if (!object3dCommon_) {
        return false;
    }

    ModelManager* modelManager = ModelManager::GetInstance();
    if (!modelManager->FindModel(kSupportActorModelPath) ||
        !modelManager->FindModel(kMultiMeshModelPath) ||
        !modelManager->FindModel(kMultiMaterialModelPath) ||
        !modelManager->FindModel("game_bonus_weapon_box") ||
        !modelManager->FindModel("game_bonus_joint_sphere") ||
        !modelManager->FindModel("game_bonus_hand_particle")) {
        return false;
    }

    supportActorObject_ = CreateObject(
        kSupportActorModelPath,
        { 0.0f, kHiddenObjectY, 0.0f },
        { 0.82f, 0.82f, 0.82f });
    multiMeshModuleObject_ = CreateObject(
        kMultiMeshModelPath,
        { 0.0f, kHiddenObjectY, 0.0f },
        { 0.15f, 0.15f, 0.15f });
    multiMaterialModuleObject_ = CreateObject(
        kMultiMaterialModelPath,
        { 0.0f, kHiddenObjectY, 0.0f },
        { 0.15f, 0.15f, 0.15f });

    if (!supportActorObject_ ||
        !multiMeshModuleObject_ ||
        !multiMaterialModuleObject_) {
        Finalize();
        return false;
    }

    supportActorObject_->SetEnvironmentCoefficient(0.16f);
    supportActorObject_->SetShadowReceiveStrength(0.0f);
    supportActorObject_->SetColor({ 0.62f, 0.96f, 1.0f, 1.0f });
    multiMeshModuleObject_->SetEnvironmentCoefficient(0.20f);
    multiMaterialModuleObject_->SetEnvironmentCoefficient(0.20f);

    if (supportActorObject_->HasSkeleton()) {
        const Skeleton& skeleton = supportActorObject_->GetSkeleton();
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

    weaponBladeObject_ = CreateObject(
        "game_bonus_weapon_box",
        { 0.0f, kHiddenObjectY, 0.0f },
        { 0.055f, 0.12f, 0.90f });
    weaponGuardObject_ = CreateObject(
        "game_bonus_weapon_box",
        { 0.0f, kHiddenObjectY, 0.0f },
        { 0.38f, 0.07f, 0.07f });
    weaponGripObject_ = CreateObject(
        "game_bonus_weapon_box",
        { 0.0f, kHiddenObjectY, 0.0f },
        { 0.09f, 0.09f, 0.28f });
    if (!weaponBladeObject_ || !weaponGuardObject_ || !weaponGripObject_) {
        Finalize();
        return false;
    }

    weaponBladeObject_->SetLightingMode(0);
    weaponBladeObject_->SetEnvironmentCoefficient(0.0f);
    weaponBladeObject_->SetColor({ 0.18f, 0.95f, 1.0f, 1.0f });
    weaponGuardObject_->SetEnvironmentCoefficient(0.22f);
    weaponGuardObject_->SetColor({ 1.0f, 0.70f, 0.18f, 1.0f });
    weaponGripObject_->SetEnvironmentCoefficient(0.08f);
    weaponGripObject_->SetColor({ 0.10f, 0.12f, 0.18f, 1.0f });

    InitializeHandParticles();
    InitializeBoneDebug();
    isReady_ =
        supportActorObject_->HasSkeleton() &&
        rightHandJointIndex_ >= 0 &&
        rightForeArmJointIndex_ >= 0 &&
        leftHandJointIndex_ >= 0;
    return isReady_;
}

void GameBonusActor::Finalize()
{
    jointDebugObjects_.clear();
    boneDebugObjects_.clear();
    for (HandParticle& particle : handParticles_) {
        particle.object.reset();
        particle.isActive = false;
    }
    handEmitterCoreObject_.reset();
    weaponGripObject_.reset();
    weaponGuardObject_.reset();
    weaponBladeObject_.reset();
    multiMaterialModuleObject_.reset();
    multiMeshModuleObject_.reset();
    supportActorObject_.reset();
    object3dCommon_ = nullptr;
    rightHandJointIndex_ = -1;
    rightForeArmJointIndex_ = -1;
    leftHandJointIndex_ = -1;
    handParticleSerial_ = 0;
    handParticleSpawnAccumulator_ = 0.0f;
    animationTime_ = 0.0f;
    showBoneDebug_ = false;
    isReady_ = false;
}

void GameBonusActor::Update(
    float deltaTime,
    const Math::Vector3& playerPosition,
    float railDistance,
    bool showBoneDebug)
{
    if (!isReady_ || !supportActorObject_) {
        return;
    }

    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 0.1f);
    animationTime_ += safeDeltaTime;
    showBoneDebug_ = showBoneDebug;

    const float hover = std::sin(animationTime_ * 2.1f) * 0.16f;
    const Math::Vector3 supportPosition{
        playerPosition.x - 3.25f,
        playerPosition.y - 0.20f + hover,
        railDistance + 4.25f,
    };
    supportActorObject_->SetTranslate(supportPosition);
    supportActorObject_->SetRotate({
        0.0f,
        std::numbers::pi_v<float>,
        std::sin(animationTime_ * 1.7f) * 0.035f,
    });
    supportActorObject_->UpdateAnimation(safeDeltaTime);
    supportActorObject_->Update();

    const float orbit = animationTime_ * 1.35f;
    multiMeshModuleObject_->SetTranslate({
        supportPosition.x - 0.72f,
        supportPosition.y + 0.72f + std::sin(orbit) * 0.10f,
        supportPosition.z - 0.18f + std::cos(orbit) * 0.12f,
    });
    multiMeshModuleObject_->SetRotate({
        orbit * 0.35f,
        orbit,
        orbit * 0.20f,
    });
    multiMeshModuleObject_->Update();

    multiMaterialModuleObject_->SetTranslate({
        supportPosition.x + 0.72f,
        supportPosition.y + 0.72f - std::sin(orbit) * 0.10f,
        supportPosition.z - 0.18f - std::cos(orbit) * 0.12f,
    });
    multiMaterialModuleObject_->SetRotate({
        -orbit * 0.25f,
        -orbit,
        orbit * 0.18f,
    });
    multiMaterialModuleObject_->Update();

    UpdateHeldWeapon();
    UpdateHandParticles(safeDeltaTime);
    if (showBoneDebug_) {
        UpdateBoneDebug();
    }
}

void GameBonusActor::Draw() const
{
    if (!isReady_) {
        return;
    }

    supportActorObject_->Draw();
    weaponGripObject_->Draw();
    weaponGuardObject_->Draw();
    weaponBladeObject_->Draw();
    multiMeshModuleObject_->Draw();
    multiMaterialModuleObject_->Draw();
    handEmitterCoreObject_->Draw();
    for (const HandParticle& particle : handParticles_) {
        if (particle.isActive && particle.object) {
            particle.object->Draw();
        }
    }

    if (showBoneDebug_) {
        for (const auto& bone : boneDebugObjects_) {
            bone->Draw();
        }
        for (const auto& joint : jointDebugObjects_) {
            joint->Draw();
        }
    }
}

std::unique_ptr<Object3d> GameBonusActor::CreateObject(
    const char* modelName,
    const Math::Vector3& translate,
    const Math::Vector3& scale)
{
    if (!object3dCommon_ || !modelName) {
        return nullptr;
    }

    auto object = std::make_unique<Object3d>();
    object->Initialize(object3dCommon_);
    object->SetModel(modelName);
    object->SetTranslate(translate);
    object->SetScale(scale);
    object->SetLightingMode(2);
    object->SetEnvironmentCoefficient(0.12f);
    object->SetShadowReceiveStrength(0.0f);
    object->Update();
    return object;
}

void GameBonusActor::InitializeBoneDebug()
{
    jointDebugObjects_.clear();
    boneDebugObjects_.clear();
    if (!supportActorObject_ || !supportActorObject_->HasSkeleton()) {
        return;
    }

    const Skeleton& skeleton = supportActorObject_->GetSkeleton();
    jointDebugObjects_.reserve(skeleton.joints.size());
    boneDebugObjects_.reserve(skeleton.joints.size());
    for (const Joint& joint : skeleton.joints) {
        auto jointObject = CreateObject(
            "game_bonus_joint_sphere",
            { 0.0f, kHiddenObjectY, 0.0f },
            { 0.020f, 0.020f, 0.020f });
        jointObject->SetLightingMode(0);
        jointObject->SetEnvironmentCoefficient(0.0f);
        jointObject->SetColor({ 0.28f, 0.95f, 1.0f, 1.0f });
        jointDebugObjects_.push_back(std::move(jointObject));

        if (joint.parent.has_value()) {
            auto boneObject = CreateObject(
                "game_bonus_weapon_box",
                { 0.0f, kHiddenObjectY, 0.0f },
                { 0.012f, 0.012f, 0.012f });
            boneObject->SetLightingMode(0);
            boneObject->SetEnvironmentCoefficient(0.0f);
            boneObject->SetColor({ 1.0f, 0.72f, 0.18f, 1.0f });
            boneDebugObjects_.push_back(std::move(boneObject));
        }
    }
}

void GameBonusActor::UpdateBoneDebug()
{
    if (!supportActorObject_ || !supportActorObject_->HasSkeleton()) {
        return;
    }

    const Skeleton& skeleton = supportActorObject_->GetSkeleton();
    const Math::Matrix4x4& worldMatrix = supportActorObject_->GetWorldMatrix();
    for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex) {
        const Math::Vector3 position = ExtractTranslation(Math::Multiply(
            skeleton.joints[jointIndex].skeletonSpaceMatrix,
            worldMatrix));
        jointDebugObjects_[jointIndex]->SetTranslate(position);
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
            jointPosition.z - parentPosition.z,
        };
        const float length = std::sqrt(
            difference.x * difference.x +
            difference.y * difference.y +
            difference.z * difference.z);
        const float safeLength = (std::max)(length, 0.001f);
        const Math::Vector3 direction{
            difference.x / safeLength,
            difference.y / safeLength,
            difference.z / safeLength,
        };

        Object3d* boneObject = boneDebugObjects_[boneIndex].get();
        boneObject->SetTranslate({
            (jointPosition.x + parentPosition.x) * 0.5f,
            (jointPosition.y + parentPosition.y) * 0.5f,
            (jointPosition.z + parentPosition.z) * 0.5f,
        });
        boneObject->SetRotate(MakeBoneRotate(direction));
        boneObject->SetScale({ 0.012f, 0.012f, safeLength });
        boneObject->Update();
        ++boneIndex;
    }
}

bool GameBonusActor::TryGetJointWorldPosition(
    int32_t jointIndex,
    Math::Vector3& position) const
{
    if (!supportActorObject_ ||
        !supportActorObject_->HasSkeleton() ||
        jointIndex < 0) {
        return false;
    }

    const Skeleton& skeleton = supportActorObject_->GetSkeleton();
    if (static_cast<size_t>(jointIndex) >= skeleton.joints.size()) {
        return false;
    }

    position = ExtractTranslation(Math::Multiply(
        skeleton.joints[static_cast<size_t>(jointIndex)].skeletonSpaceMatrix,
        supportActorObject_->GetWorldMatrix()));
    return true;
}

void GameBonusActor::UpdateHeldWeapon()
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
        handPosition.z - foreArmPosition.z,
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
        armDirection.z / armLength,
    };
    const Math::Vector3 rotate = MakeBoneRotate(direction);
    const auto pointAlongWeapon = [&](float distance) {
        return Math::Vector3{
            handPosition.x + direction.x * distance,
            handPosition.y + direction.y * distance,
            handPosition.z + direction.z * distance,
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

void GameBonusActor::InitializeHandParticles()
{
    handParticleSerial_ = 0;
    handParticleSpawnAccumulator_ = 0.0f;
    handEmitterCoreObject_ = CreateObject(
        "game_bonus_hand_particle",
        { 0.0f, kHiddenObjectY, 0.0f },
        { 0.065f, 0.065f, 0.065f });
    handEmitterCoreObject_->SetLightingMode(0);
    handEmitterCoreObject_->SetEnvironmentCoefficient(0.0f);
    handEmitterCoreObject_->SetColor({ 0.26f, 0.92f, 1.0f, 1.0f });

    for (HandParticle& particle : handParticles_) {
        particle.object = CreateObject(
            "game_bonus_hand_particle",
            { 0.0f, kHiddenObjectY, 0.0f },
            { 0.025f, 0.025f, 0.025f });
        particle.object->SetLightingMode(0);
        particle.object->SetEnvironmentCoefficient(0.0f);
        particle.object->SetColor({ 0.20f, 0.92f, 1.0f, 1.0f });
        particle.isActive = false;
    }
}

void GameBonusActor::UpdateHandParticles(float deltaTime)
{
    Math::Vector3 emitterPosition{};
    if (!TryGetJointWorldPosition(leftHandJointIndex_, emitterPosition)) {
        return;
    }

    const float corePulse =
        0.060f + 0.012f * std::sin(animationTime_ * 8.0f);
    handEmitterCoreObject_->SetTranslate(emitterPosition);
    handEmitterCoreObject_->SetScale({ corePulse, corePulse, corePulse });
    handEmitterCoreObject_->Update();

    handParticleSpawnAccumulator_ += deltaTime;
    constexpr float kSpawnInterval = 0.03f;
    while (handParticleSpawnAccumulator_ >= kSpawnInterval) {
        handParticleSpawnAccumulator_ -= kSpawnInterval;
        SpawnHandParticle(emitterPosition);
    }

    for (HandParticle& particle : handParticles_) {
        if (!particle.isActive) {
            continue;
        }

        particle.age += deltaTime;
        if (particle.age >= particle.lifetime) {
            particle.isActive = false;
            particle.object->SetTranslate({ 0.0f, kHiddenObjectY, 0.0f });
            particle.object->Update();
            continue;
        }

        Math::Vector3 position = particle.object->GetTranslate();
        position.x += particle.velocity.x * deltaTime;
        position.y += particle.velocity.y * deltaTime;
        position.z += particle.velocity.z * deltaTime;
        particle.velocity.y += 0.16f * deltaTime;

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
            animationTime_ * 1.4f,
            animationTime_ * 1.8f,
            0.0f,
        });
        particle.object->Update();
    }
}

void GameBonusActor::SpawnHandParticle(
    const Math::Vector3& emitterPosition)
{
    HandParticle* availableParticle = nullptr;
    for (HandParticle& particle : handParticles_) {
        if (!particle.isActive) {
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

    availableParticle->isActive = true;
    availableParticle->age = 0.0f;
    availableParticle->lifetime = 0.78f + variation * 0.20f;
    availableParticle->velocity = {
        std::cos(phase) * (0.18f + variation * 0.10f),
        0.20f + variation * 0.14f,
        std::sin(phase) * (0.15f + variation * 0.09f),
    };
    availableParticle->object->SetTranslate({
        emitterPosition.x + std::cos(phase) * radius,
        emitterPosition.y + 0.02f,
        emitterPosition.z + std::sin(phase) * radius,
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
