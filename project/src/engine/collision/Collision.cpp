#include "engine/collision/Collision.h"

Collision::Aabb Collision::MakeAabb(
    const Math::Vector3& center,
    const Math::Vector3& halfExtents)
{
    return {
        {
            center.x - halfExtents.x,
            center.y - halfExtents.y,
            center.z - halfExtents.z,
        },
        {
            center.x + halfExtents.x,
            center.y + halfExtents.y,
            center.z + halfExtents.z,
        },
    };
}

bool Collision::Intersects(const Aabb& first, const Aabb& second)
{
    return first.min.x <= second.max.x && first.max.x >= second.min.x &&
        first.min.y <= second.max.y && first.max.y >= second.min.y &&
        first.min.z <= second.max.z && first.max.z >= second.min.z;
}

bool Collision::OverlapsXZ(const Aabb& first, const Aabb& second)
{
    return first.min.x <= second.max.x && first.max.x >= second.min.x &&
        first.min.z <= second.max.z && first.max.z >= second.min.z;
}
