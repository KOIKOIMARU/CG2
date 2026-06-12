#pragma once

#include "app/Bullet.h"
#include "app/Enemy.h"
#include "app/Player.h"
#include "engine/3d/Camera.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Object3dCommon.h"

#include <list>
#include <memory>
#include <vector>

class DirectXCommon;
class ImGuiManager;
class Input;
class Model;
class Skybox;
class SpriteCommon;
class SrvManager;

class GameRuntime {
public:
    GameRuntime();
    ~GameRuntime();

    void SetSystems(
        DirectXCommon* dxCommon,
        SrvManager* srvManager,
        SpriteCommon* spriteCommon,
        ImGuiManager* imguiManager,
        Input* input);
    void Initialize();
    void Finalize();
    void Update();
    void Draw();
    void SetDebugGuiAllowed(bool isAllowed);
    void SetRenderingOptions(bool showSkybox, int postEffectMode);
    int GetPostEffectMode() const;
    bool IsExitRequested() const { return isExitRequested_; }
    int GetPlayerHp() const;
    int GetPlayerMaxHp() const { return 100; }

private:
    struct ExplosionEffect {
        Math::Vector3 worldPosition{};
        int age = 0;
        int duration = 24;
        float strength = 1.0f;
        bool isCoinCollect = false;
    };

    struct EnvironmentBonusCoin {
        std::unique_ptr<Object3d> object;
        Math::Vector3 basePosition{};
        float collisionRadius = 0.95f;
        float rotationSpeed = 0.035f;
        float floatPhase = 0.0f;
        bool isActive = true;
    };

    void FirePlayerBullet();
    void FireEnemyBullet(const Math::Vector3& position);
    void SpawnEnemy();
    void InitializeEnvironmentBonusCoins();
    void UpdateEnvironmentBonusCoins();
    void UpdateEnemyWave();
    void AdvanceEnemyWaveIfCleared();
    void UpdateLockOnTarget();
    bool TryProjectToScreen(
        const Math::Vector3& worldPosition,
        Math::Vector2& screenPosition) const;
    void AddExplosionEffect(
        const Math::Vector3& worldPosition,
        float strength = 1.0f);
    void AddCoinCollectEffect(const Math::Vector3& worldPosition);
    void UpdateExplosionEffects();
    void DrawExplosionEffects();
    void DrawHud();
    void DrawLockOnHud();
    void DrawResultOverlay();
    void UpdatePlayerBullets();
    void UpdateEnemyBullets();
    void UpdateEnemies();
    void CheckBulletBonusCoinCollisions();
    void CheckPlayerBonusCoinCollisions();
    void CheckBulletEnemyCollisions();
    void CheckEnemyBulletPlayerCollisions();
    void UpdateGameCamera();
    void AddCameraShake(float power, int duration);
    void LoadSceneObjects(const char* path);

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    SpriteCommon* spriteCommon_ = nullptr;
    ImGuiManager* imguiManager_ = nullptr;
    Input* input_ = nullptr;

    std::unique_ptr<Object3dCommon> object3dCommon_;
    std::unique_ptr<Skybox> skybox_;
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<Player> player_;
    std::vector<std::unique_ptr<Object3d>> sceneObjects_;
    std::list<std::unique_ptr<Bullet>> playerBullets_;
    std::list<std::unique_ptr<Bullet>> enemyBullets_;
    std::list<std::unique_ptr<Enemy>> enemies_;
    std::vector<ExplosionEffect> explosionEffects_;
    std::vector<EnvironmentBonusCoin> environmentBonusCoins_;

    Model* playerModel_ = nullptr;
    Model* bulletModel_ = nullptr;
    Model* enemyModel_ = nullptr;

    int shootCooldown_ = 0;
    int enemySpawnTimer_ = 0;
    int enemyShotTimer_ = 60;
    int currentWaveIndex_ = 0;
    int spawnedEnemyCountInWave_ = 0;
    int spawnSequenceIndex_ = 0;
    int score_ = 0;
    int defeatedEnemyCount_ = 0;
    int resultTransitionTimer_ = -1;
    int chargeTimer_ = 0;
    int chargeFlashTimer_ = 0;
    int cameraShakeTimer_ = 0;
    int cameraShakeDuration_ = 1;
    float cameraTimer_ = 0.0f;
    float cameraShakePower_ = 0.0f;
    float railDistance_ = 0.0f;
    float railSpeed_ = 0.045f;
    Math::Vector3 cameraTranslate_{ 0.0f, 2.5f, -13.0f };
    Math::Vector3 previousPlayerTranslate_{ 0.0f, 0.0f, 0.0f };
    const Enemy* lockedEnemy_ = nullptr;
    Math::Vector2 lockedEnemyScreen_{ 0.0f, 0.0f };
    Math::Vector2 reticleScreen_{ 0.0f, 0.0f };
    bool hasLockTarget_ = false;
    int postEffectMode_ = 12;
    bool isGameOver_ = false;
    bool isGameClear_ = false;
    bool isDebugGuiVisible_ = true;
    bool isDebugGuiAllowed_ = true;
    bool isExitRequested_ = false;
    bool showSkybox_ = true;
};
