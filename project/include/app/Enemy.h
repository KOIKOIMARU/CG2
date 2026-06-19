#pragma once

#include "engine/3d/Object3d.h"
#include "engine/base/Math.h"

#include <memory>

class Model;
class Object3dCommon;

class Enemy {
public:
    enum class Behavior {
        Formation,
        Swoop,
        StrafeShooter
    };

    enum class EntryStyle {
        Direct,
        VFormation,
        LeftSweep,
        RightSweep,
        PopShooter
    };

    enum class LifeState {
        Alive,
        Destroyed,
        Escaped
    };

    void Initialize(
        Object3dCommon* object3dCommon,
        Model* model,
        const Math::Vector3& position,
        Behavior behavior,
        EntryStyle entryStyle = EntryStyle::Direct);
    void Update(float railDistance);
    void Draw();

    void Kill() { lifeState_ = LifeState::Destroyed; }
    bool CanShoot() const;
    bool IsDead() const { return lifeState_ != LifeState::Alive; }
    bool WasDestroyed() const { return lifeState_ == LifeState::Destroyed; }
    bool HasEscaped() const { return lifeState_ == LifeState::Escaped; }
    const Math::Vector3& GetTranslate() const { return translate_; }
    float GetRadius() const { return collisionRadius_; }

private:
    void Escape() { lifeState_ = LifeState::Escaped; }

    std::unique_ptr<Object3d> object_;
    Math::Vector3 baseTranslate_{};
    Math::Vector3 translate_{};
    int age_ = 0;
    float moveTimer_ = 0.0f;
    float phase_ = 0.0f;
    float horizontalAmplitude_ = 1.2f;
    float verticalAmplitude_ = 0.55f;
    float collisionRadius_ = 0.8f;
    Behavior behavior_ = Behavior::Formation;
    EntryStyle entryStyle_ = EntryStyle::Direct;
    LifeState lifeState_ = LifeState::Alive;
};
