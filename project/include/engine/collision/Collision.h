#pragma once

#include "engine/base/Math.h"

namespace Collision {

// 軸に平行な直方体。当たり判定の境界はワールド座標で保持する。
struct Aabb {
    Math::Vector3 min{}; // X/Y/Z各軸の最小境界
    Math::Vector3 max{}; // X/Y/Z各軸の最大境界
};

// 中心位置と各軸の半分の大きさからAABBを生成する。
Aabb MakeAabb(
    const Math::Vector3& center,
    const Math::Vector3& halfExtents);

// 境界が接している状態も衝突として扱う。
bool Intersects(const Aabb& first, const Aabb& second);

// XZ平面だけの重なりを調べる。足場への着地候補判定に使用できる。
bool OverlapsXZ(const Aabb& first, const Aabb& second);

} // namespace Collision
