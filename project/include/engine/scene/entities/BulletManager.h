#pragma once
#include <vector>
#include "engine/scene/entities/Bullet.h"

class Boss;
struct AABB;

class BulletManager {
public:
    void Clear();

    // 生成（PlayerControllerから）
    void Spawn(
        const Math::Vector3& pos,
        const Math::Vector3& vel,
        float power,
        float damage,
        float life
    );

    void Update(float dt);

    // ボスとの当たり判定
    void CheckHitBoss(Boss& boss);

    const std::vector<Bullet>& GetBullets() const { return bullets_; }

private:
    std::vector<Bullet> bullets_;
};
