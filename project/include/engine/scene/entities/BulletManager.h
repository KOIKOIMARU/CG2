#pragma once
#include <array>
#include "engine/scene/entities/Bullet.h"

class Boss;

class BulletManager {
public:
    static constexpr size_t kMaxBullets = 128;

    void Clear();
    void Spawn(const Math::Vector3& pos, const Math::Vector3& vel,
        float power, float damage, float life);

    void Update(float dt);
    void CheckHitBoss(Boss& boss);

    const std::array<Bullet, kMaxBullets>& GetBullets() const { return bullets_; }

private:
    std::array<Bullet, kMaxBullets> bullets_{};
};
