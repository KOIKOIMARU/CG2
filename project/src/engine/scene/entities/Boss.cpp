#include "engine/scene/entities/Boss.h"

void Boss::Update(float dt) {} // 空実装（非推奨）

void Boss::Damage(float dmg)
{
    if (!alive_) return;

    hp_ -= dmg;
    if (hp_ <= 0.0f) {
        hp_ = 0.0f;
        alive_ = false;
    }
}
