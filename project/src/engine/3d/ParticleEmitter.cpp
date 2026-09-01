#include "engine/3d/ParticleEmitter.h"

#include "engine/3d/ParticleManager.h"

using namespace Math;

ParticleEmitter::ParticleEmitter(
    const std::string& groupName,
    const Vector3& position,
    float emitInterval,
    uint32_t emitCount
)
    : groupName_(groupName),
    position_(position),
    emitInterval_(emitInterval),
    emitCount_(emitCount),
    elapsedTime_(0.0f)
{
}

void ParticleEmitter::Update(float deltaTime)
{
    elapsedTime_ += deltaTime;

    // 余剰時間を残すことで、フレーム時間が揺れても長期的な発生数を一定に保つ。
    while (elapsedTime_ >= emitInterval_) {
        Emit();
        elapsedTime_ -= emitInterval_;
    }
}

void ParticleEmitter::Emit()
{
    ParticleManager::GetInstance()->Emit(
        groupName_,
        position_,
        emitCount_
    );
}
