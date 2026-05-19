#pragma once

#include "engine/3d/Object3d.h"
#include "engine/base/Math.h"

#include <memory>

class Model;
class Object3dCommon;

class Enemy {
public:
    void Initialize(
        Object3dCommon* object3dCommon,
        Model* model,
        const Math::Vector3& position);
    void Update();
    void Draw();

    void Kill() { isDead_ = true; }
    bool IsDead() const { return isDead_; }
    const Math::Vector3& GetTranslate() const { return translate_; }
    float GetRadius() const { return collisionRadius_; }

private:
    std::unique_ptr<Object3d> object_;
    Math::Vector3 translate_{};
    float speed_ = 0.08f;
    float collisionRadius_ = 0.8f;
    bool isDead_ = false;
};
