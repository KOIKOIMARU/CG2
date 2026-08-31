#pragma once

#include "engine/3d/Object3d.h"
#include "engine/base/Math.h"

#include <array>
#include <memory>

class Model;
class Object3dCommon;

// レールシューティングサンプル専用の弾丸。
// 本体・発光・軌跡の表示と寿命、貫通、追尾、当たり済み対象をまとめて管理する。
class Bullet {
public:
    // 描画に使用するModel群はすべて借用し、GameRuntime側が所有する。
    void Initialize(
        Object3dCommon* object3dCommon,
        Model* model,
        const Math::Vector3& position,
        const Math::Vector3& velocity,
        const Math::Vector4& color = { 1.0f, 0.85f, 0.25f, 1.0f },
        int lifeTimer = 180,
        const Math::Vector3& scale = { 0.34f, 0.34f, 0.95f },
        float collisionRadius = 0.46f,
        int hitLimit = 1,
        Model* glowModel = nullptr,
        const Math::Vector4& glowColor = { 1.0f, 0.85f, 0.25f, 0.55f },
        const Math::Vector3& glowScale = { 0.85f, 0.85f, 1.0f },
        Model* trailModel = nullptr,
        const Math::Vector4& trailColor = { 1.0f, 0.85f, 0.25f, 0.45f },
        const Math::Vector3& trailScale = { 0.42f, 1.75f, 1.0f },
        float trailOffset = 0.9f,
        Model* sparkleModel = nullptr,
        const Math::Vector4& sparkleColor = { 1.0f, 0.95f, 0.55f, 0.45f },
        const Math::Vector3& sparkleScale = { 0.12f, 0.12f, 1.0f },
        int damage = 8);
    void Update(float timeScale = 1.0f);
    void Draw();
    void DrawGlow(const Math::Vector3& cameraRotate);

    bool IsDead() const { return isDead_; }
    void Kill() { isDead_ = true; }
    void RegisterHit();
    // 同じ対象への多重ヒットを防ぐ。初回登録時だけtrueを返す。
    bool TryRegisterHitTarget(const void* target);
    void EnableHoming(float strength);
    void SetHomingTarget(const Math::Vector3& target);
    void ClearHomingTarget() { hasHomingTarget_ = false; }
    bool CanHome() const { return homingEnabled_; }
    void SetFeverShot(bool isFeverShot) { isFeverShot_ = isFeverShot; }
    bool IsFeverShot() const { return isFeverShot_; }
    const Math::Vector3& GetTranslate() const { return translate_; }
    const Math::Vector3& GetVelocity() const { return velocity_; }
    float GetRadius() const { return collisionRadius_; }
    int GetDamage() const { return damage_; }

private:
    std::unique_ptr<Object3d> object_;
    std::unique_ptr<Object3d> glowObject_;
    std::unique_ptr<Object3d> trailObject_;
    std::array<std::unique_ptr<Object3d>, 2> sparkleObjects_;
    Math::Vector3 startTranslate_{};
    Math::Vector3 translate_{};
    Math::Vector3 velocity_{ 0.0f, 0.0f, 0.5f };
    Math::Vector3 bodyBaseScale_{ 0.34f, 0.34f, 0.95f };
    Math::Vector3 glowBaseScale_{ 0.85f, 0.85f, 1.0f };
    Math::Vector3 trailBaseScale_{ 0.42f, 1.75f, 1.0f };
    Math::Vector3 trailOffset_{ 0.0f, 0.0f, -0.9f };
    Math::Vector4 bodyBaseColor_{ 1.0f, 0.85f, 0.25f, 1.0f };
    Math::Vector4 glowBaseColor_{ 1.0f, 0.85f, 0.25f, 0.55f };
    Math::Vector4 trailBaseColor_{ 1.0f, 0.85f, 0.25f, 0.45f };
    Math::Vector4 sparkleBaseColor_{ 1.0f, 0.95f, 0.55f, 0.45f };
    Math::Vector3 sparkleBaseScale_{ 0.12f, 0.12f, 1.0f };
    float trailDistance_ = 0.9f;
    float trailRoll_ = 0.0f;
    float collisionRadius_ = 0.46f;
    float maxTravelDistance_ = 90.0f;
    int age_ = 0;
    int lifeTimer_ = 180;
    int initialLifeTimer_ = 180;
    float lifeTimerFloat_ = 180.0f;
    int damage_ = 8;
    int remainingHits_ = 1;
    std::array<const void*, 4> hitTargets_{};
    float homingStrength_ = 0.0f;
    bool homingEnabled_ = false;
    bool hasHomingTarget_ = false;
    Math::Vector3 homingTarget_{};
    bool isFeverShot_ = false;
    bool isDead_ = false;
};
