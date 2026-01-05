#include "engine/scene/entities/BulletManager.h"
#include "engine/scene/entities/Boss.h"
#include "engine/scene/world/Collision.h"

void BulletManager::Clear()
{
    for (auto& b : bullets_) {
        b = Bullet{};
        b.alive = false;
    }
}

void BulletManager::Spawn(const Math::Vector3& pos, const Math::Vector3& vel,
    float power, float damage, float life)
{
    // 空きスロット探す
    for (auto& b : bullets_) {
        if (!b.alive) {
            b.pos = pos;
            b.vel = vel;
            b.size = { power, power, 1.0f };
            b.damage = damage;
            b.life = life;
            b.alive = true;
            return;
        }
    }
    // 空き無しなら捨てる（上限）
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

        AABB aB{ b.pos.x, b.pos.y, b.size.x, b.size.y };

        if (Collision::IntersectAABB(aB, aBoss)) {
            boss.Damage(b.damage);
            b.alive = false;
        }
    }
}
