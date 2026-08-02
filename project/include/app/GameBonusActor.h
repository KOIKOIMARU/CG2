#pragma once

#include "engine/base/Math.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

class Object3d;
class Object3dCommon;

class GameBonusActor {
public:
    bool Initialize(Object3dCommon* object3dCommon);
    void Finalize();
    void Update(
        float deltaTime,
        const Math::Vector3& playerPosition,
        float railDistance,
        bool showBoneDebug);
    void Draw() const;

    bool IsReady() const { return isReady_; }

private:
    struct HandParticle {
        std::unique_ptr<Object3d> object;
        Math::Vector3 velocity{};
        float age = 0.0f;
        float lifetime = 1.0f;
        bool isActive = false;
    };

    static constexpr size_t kHandParticlePoolSize = 32;

    std::unique_ptr<Object3d> CreateObject(
        const char* modelName,
        const Math::Vector3& translate,
        const Math::Vector3& scale);
    void InitializeBoneDebug();
    void UpdateBoneDebug();
    bool TryGetJointWorldPosition(
        int32_t jointIndex,
        Math::Vector3& position) const;
    void UpdateHeldWeapon();
    void InitializeHandParticles();
    void UpdateHandParticles(float deltaTime);
    void SpawnHandParticle(const Math::Vector3& emitterPosition);

private:
    Object3dCommon* object3dCommon_ = nullptr;
    std::unique_ptr<Object3d> supportActorObject_;
    std::unique_ptr<Object3d> multiMeshModuleObject_;
    std::unique_ptr<Object3d> multiMaterialModuleObject_;
    std::unique_ptr<Object3d> weaponBladeObject_;
    std::unique_ptr<Object3d> weaponGuardObject_;
    std::unique_ptr<Object3d> weaponGripObject_;
    std::unique_ptr<Object3d> handEmitterCoreObject_;
    std::array<HandParticle, kHandParticlePoolSize> handParticles_{};
    std::vector<std::unique_ptr<Object3d>> jointDebugObjects_;
    std::vector<std::unique_ptr<Object3d>> boneDebugObjects_;
    int32_t rightHandJointIndex_ = -1;
    int32_t rightForeArmJointIndex_ = -1;
    int32_t leftHandJointIndex_ = -1;
    uint32_t handParticleSerial_ = 0;
    float handParticleSpawnAccumulator_ = 0.0f;
    float animationTime_ = 0.0f;
    bool showBoneDebug_ = false;
    bool isReady_ = false;
};
