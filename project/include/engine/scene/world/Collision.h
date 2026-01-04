// Collision.h
#pragma once
#include <algorithm>

struct AABB {
    float x, y, w, h; // 左下(x,y) 幅w 高さh
};

class Collision {
public:
    static bool IntersectAABB(const AABB& a, const AABB& b) {
        return !(a.x + a.w < b.x || b.x + b.w < a.x ||
            a.y + a.h < b.y || b.y + b.h < a.y);
    }
};

