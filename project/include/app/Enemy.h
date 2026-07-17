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
        StrafeShooter,
        DiveBomber,
        Crossfire,
        Sniper,
        Shield,
        Support,
        Boss
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
        EntryStyle entryStyle = EntryStyle::Direct,
        int maxHpOverride = 0,
        float scaleMultiplier = 1.0f,
        const char* textureOverride = nullptr);
    void Update(float railDistance, float timeScale = 1.0f);
    void Draw();
    void DrawShadow(const Math::Matrix4x4& lightViewProjection);

    void Kill();
    bool Damage(int damage);
    bool CanShoot() const;
    bool IsDead() const { return lifeState_ == LifeState::Destroyed || lifeState_ == LifeState::Escaped; }
    bool WasDestroyed() const { return lifeState_ == LifeState::Destroyed; }
    bool HasEscaped() const { return lifeState_ == LifeState::Escaped; }
    bool IsBoss() const { return behavior_ == Behavior::Boss; }
    bool IsCrossfire() const { return behavior_ == Behavior::Crossfire; }
    bool IsSniper() const { return behavior_ == Behavior::Sniper; }
    bool IsShield() const { return behavior_ == Behavior::Shield; }
    bool IsSupport() const { return behavior_ == Behavior::Support; }
    bool HasShield() const { return shieldHp_ > 0; }
    int GetShieldHp() const { return shieldHp_; }
    int GetShieldMaxHp() const { return shieldMaxHp_; }
    bool IsTargetable() const;
    int GetHp() const { return hp_; }
    int GetMaxHp() const { return maxHp_; }
    const Math::Vector3& GetTranslate() const { return translate_; }
    Math::Vector3 GetAimPosition() const;
    float GetRadius() const { return collisionRadius_; }
    float GetAimRadius() const;

private:
    void Escape() { lifeState_ = LifeState::Escaped; }

    std::unique_ptr<Object3d> object_;
    Math::Vector3 baseTranslate_{};
    Math::Vector3 translate_{};
    Math::Vector3 baseScale_{ 1.0f, 1.0f, 1.0f };
    Math::Vector3 aimLocalCenter_{ 0.0f, 0.0f, 0.0f };
    Math::Vector4 baseColor_{ 1.0f, 0.25f, 0.25f, 1.0f };
    int age_ = 0;
    float ageTimer_ = 0.0f;
    int hp_ = 1;
    int maxHp_ = 1;
    int shieldHp_ = 0;
    int shieldMaxHp_ = 0;
    int hitFlashTimer_ = 0;
    int hitFlashDuration_ = 1;
    float hitFlashStrength_ = 1.0f;
    float moveTimer_ = 0.0f;
    float phase_ = 0.0f;
    float horizontalAmplitude_ = 1.2f;
    float verticalAmplitude_ = 0.55f;
    float collisionRadius_ = 0.8f;
    float aimRadius_ = 0.8f;
    float visualScaleRate_ = 1.0f;
    Behavior behavior_ = Behavior::Formation;
    EntryStyle entryStyle_ = EntryStyle::Direct;
    LifeState lifeState_ = LifeState::Alive;
};
