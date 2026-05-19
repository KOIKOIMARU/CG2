#pragma once

#include "engine/3d/Object3d.h"
#include "engine/base/Math.h"

#include <memory>

class Input;
class Model;
class Object3dCommon;

class Player {
public:
    void Initialize(Object3dCommon* object3dCommon, Model* model);
    void Update(Input* input);
    void Draw();

    const Math::Vector3& GetTranslate() const { return translate_; }
    int GetHp() const { return hp_; }
    float GetRadius() const { return collisionRadius_; }
    bool IsDead() const { return hp_ <= 0; }
    void Damage(int amount);

private:
    std::unique_ptr<Object3d> object_;
    Math::Vector3 translate_{ 0.0f, 0.0f, 0.0f };
    float moveSpeed_ = 0.18f;
    float collisionRadius_ = 0.7f;
    int hp_ = 100;
};
