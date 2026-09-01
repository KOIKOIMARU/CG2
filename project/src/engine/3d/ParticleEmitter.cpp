#include "engine/3d/ParticleEmitter.h"

#include "engine/3d/ParticleManager.h"

using namespace Math;

// =========================
// コンストラクタ
// =========================
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

// =========================
// 更新
// =========================
void ParticleEmitter::Update(float deltaTime)
{
    elapsedTime_ += deltaTime;

    // 発生間隔を超えている間、発生し続ける
    while (elapsedTime_ >= emitInterval_) {
        Emit();
        elapsedTime_ -= emitInterval_; // 余剰時間を残し、フレーム時間が揺れても平均発生間隔を保つ。
    }
}

// =========================
// Emit（資料どおり超シンプル）
// =========================
void ParticleEmitter::Emit()
{
    ParticleManager::GetInstance()->Emit(
        groupName_,
        position_,
        emitCount_
    );
}
