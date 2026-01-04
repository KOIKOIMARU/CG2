#include "engine/scene/entities/BulletManager.h"
#include "engine/scene/entities/Boss.h"
#include <algorithm>
#include "engine/scene/world/Collision.h"


void BulletManager::Clear()
{
    bullets_.clear();
}

void BulletManager::Spawn(
    const Math::Vector3& pos,
    const Math::Vector3& vel,
    float power,
    float damage,
    float life
) {
    Bullet b;
    b.pos = pos;
    b.vel = vel;
    b.size = { power, power, 1.0f };
    b.damage = damage;
    b.life = life;
    b.alive = true;

    bullets_.push_back(b);
}

void BulletManager::Update(float dt)
{
    for (auto& b : bullets_) {
        if (!b.alive) continue;

        b.pos += b.vel * dt;
        b.life -= dt;

        if (b.life <= 0.0f) {
            b.alive = false;
        }
    }

    // 死んだ弾を消す
    bullets_.erase(
        std::remove_if(bullets_.begin(), bullets_.end(),
            [](const Bullet& b) { return !b.alive; }),
        bullets_.end()
    );
}

void BulletManager::CheckHitBoss(Boss& boss)
{
    if (!boss.IsAlive()) return;

    AABB aBoss{
        boss.GetPos().x,
        boss.GetPos().y,
        boss.GetSizeX(),
        boss.GetSizeY()
    };

    for (auto& b : bullets_) {
        if (!b.alive) continue;

        AABB aB{
            b.pos.x,
            b.pos.y,
            b.size.x,
            b.size.y
        };

        if (Collision::IntersectAABB(aB, aBoss)) {
            boss.Damage(b.damage);
            b.alive = false;
        }
    }
}
