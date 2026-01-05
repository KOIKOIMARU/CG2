#pragma once
#include <array>
#include "engine/base/Math.h"

class Boss;

class BulletManager {
public:
    static constexpr size_t kMaxBullets = 128;

    struct Bullet {
        Math::Vector3 pos{};
        Math::Vector3 vel{};
        Math::Vector3 size{ 0.3f, 0.3f, 1.0f };
        float life = 1.0f;
        float damage = 1.0f;
        bool  alive = false;
    };

    void Clear();

    void Spawn(const Math::Vector3& pos, const Math::Vector3& vel,
        float power, float damage, float life);

    void Update(float dt);
    void CheckHitBoss(Boss& boss);

    const std::array<Bullet, kMaxBullets>& GetBullets() const { return bullets_; }

private:
    std::array<Bullet, kMaxBullets> bullets_{};
};
