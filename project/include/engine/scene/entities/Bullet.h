#pragma once
#include "engine/base/Math.h"

struct Bullet {
    Math::Vector3 pos{};
    Math::Vector3 vel{};
    Math::Vector3 size{ 0.3f, 0.3f, 1.0f };

    float life = 1.0f;
    float damage = 1.0f;
    bool  alive = false;
};
