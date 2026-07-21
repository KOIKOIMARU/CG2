#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "engine/base/Math.h"
#include "engine/scene/BaseScene.h"

class Camera;
class Object3d;
class Object3dCommon;
class Skybox;

class BonusShowcaseScene : public BaseScene {
public:
    BonusShowcaseScene();
    ~BonusShowcaseScene() override;

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;

private:
    struct HandParticle {
        std::unique_ptr<Object3d> object;
        Math::Vector3 velocity{};
        float age = 0.0f;
        float lifetime = 1.0f;
        bool active = false;
    };

    static constexpr size_t kHandParticlePoolSize = 32;

    std::unique_ptr<Object3d> CreateDisplayObject(
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
    void InitializeGpuParticles();
    void UpdateGpuParticles(float deltaTime);
    void DrawShowcaseHud();

private:
    std::unique_ptr<Object3dCommon> object3dCommon_;
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<Skybox> skybox_;
    std::unique_ptr<Object3d> simpleSkinObject_;
    std::unique_ptr<Object3d> humanWalkObject_;
    std::unique_ptr<Object3d> multiMeshObject_;
    std::unique_ptr<Object3d> multiMaterialObject_;
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
    float gpuFountainSpawnAccumulator_ = 0.0f;
    float gpuSparkSpawnAccumulator_ = 0.0f;
    bool gpuParticlesEnabled_ = true;
    bool showBoneDebug_ = true;
    float demonstrationTime_ = 0.0f;
};
