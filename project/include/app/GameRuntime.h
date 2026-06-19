#pragma once

#include "app/Bullet.h"
#include "app/Enemy.h"
#include "app/Player.h"
#include "engine/3d/Camera.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/scene/SceneSerializer.h"

#include <array>
#include <list>
#include <memory>
#include <string>
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
    void SetHudViewportRect(bool isEnabled, const Math::Vector2& min, const Math::Vector2& size);
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
        int scoreValue = 0;
        bool isEnemyBurst = false;
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

    struct WaveTuning {
        int enemyCount = 6;
        int spawnInterval = 80;
        float spawnLeadDistance = 26.0f;
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
    bool HandleRuntimeShortcuts();
    void UpdateRailProgress();
    void UpdatePlayerAndCamera();
    void UpdatePlayerShooting();
    void UpdateEnemyActions();
    void UpdateWorldEntities();
    void UpdateGameplayCollisions();
    void UpdateResultAndSceneObjects();
    void UpdateExplosionEffects();
    void DrawExplosionEffects();
    void DrawEditorOverlayGuiRich();
    void DrawHud();
    void DrawLockOnHud();
    void DrawResultOverlay();
    void GetEffectiveHudViewportRect(Math::Vector2& min, Math::Vector2& size) const;
    void UpdatePlayerBullets();
    void UpdateEnemyBullets();
    void UpdateEnemies();
    int GetTotalEnemyTargetCount() const;
    void CheckBulletBonusCoinCollisions();
    void CheckPlayerBonusCoinCollisions();
    void CheckBulletEnemyCollisions();
    void CheckEnemyBulletPlayerCollisions();
    void UpdateGameCamera();
    void AddCameraShake(float power, int duration);
    std::vector<SceneSerializer::ObjectRecord> BuildRuntimeSceneRecords() const;
    SceneSerializer::SceneSettings BuildRuntimeSceneSettings() const;
    bool LoadSceneObjects(const char* path);
    bool SaveSceneObjects(const char* path);

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
    std::vector<SceneSerializer::ObjectRecord> sceneObjectRecords_;
    std::list<std::unique_ptr<Bullet>> playerBullets_;
    std::list<std::unique_ptr<Bullet>> enemyBullets_;
    std::list<std::unique_ptr<Enemy>> enemies_;
    std::vector<ExplosionEffect> explosionEffects_;
    std::vector<EnvironmentBonusCoin> environmentBonusCoins_;
    std::array<WaveTuning, 3> waveTuning_{ {
        { 6, 80, 26.0f },
        { 8, 60, 30.0f },
        { 10, 45, 34.0f }
    } };

    Model* playerModel_ = nullptr;
    Model* bulletModel_ = nullptr;
    Model* enemyModel_ = nullptr;

    int shootCooldown_ = 0;
    int enemySpawnTimer_ = 0;
    int enemyShotTimer_ = 60;
    int currentWaveIndex_ = 0;
    int spawnedEnemyCountInWave_ = 0;
    int defeatedEnemyCountInWave_ = 0;
    int spawnSequenceIndex_ = 0;
    int score_ = 0;
    int defeatedEnemyCount_ = 0;
    int resultTransitionTimer_ = -1;
    int chargeTimer_ = 0;
    int chargeFlashTimer_ = 0;
    int cameraShakeTimer_ = 0;
    int cameraShakeDuration_ = 1;
    int chargeShotThreshold_ = 70;
    int normalShootCooldown_ = 8;
    int chargedShootCooldown_ = 14;
    int enemyShotInterval_ = 75;
    int waveStartDelay_ = 90;
    float cameraTimer_ = 0.0f;
    float cameraShakePower_ = 0.0f;
    float railDistance_ = 0.0f;
    float railSpeed_ = 0.045f;
    float playerBulletSpeed_ = 0.65f;
    float lockBulletSpeed_ = 0.82f;
    float chargedBulletSpeedMultiplier_ = 1.18f;
    float enemyBulletSpeed_ = 0.28f;
    float lockRadius_ = 118.0f;
    Math::Vector2 hudViewportMin_{ 0.0f, 0.0f };
    Math::Vector2 hudViewportSize_{ 0.0f, 0.0f };
    Math::Vector2 editorOverlayViewportMin_{ 0.0f, 0.0f };
    Math::Vector2 editorOverlayViewportSize_{ 0.0f, 0.0f };
    Math::Vector3 cameraTranslate_{ 0.0f, 2.5f, -13.0f };
    Math::Vector3 previousPlayerTranslate_{ 0.0f, 0.0f, 0.0f };
    const Enemy* lockedEnemy_ = nullptr;
    Math::Vector2 lockedEnemyScreen_{ 0.0f, 0.0f };
    Math::Vector2 reticleScreen_{ 0.0f, 0.0f };
    std::string currentSceneFilePath_{ "resources/game_scene.json" };
    std::string editorStatusMessage_{ "Ready." };
    bool hasLockTarget_ = false;
    int postEffectMode_ = 12;
    bool isGameOver_ = false;
    bool isGameClear_ = false;
    bool isEditorOverlayVisible_ = false;
    bool isExitRequested_ = false;
    bool showSkybox_ = true;
    bool isHudViewportRectEnabled_ = false;
    bool hasEditorOverlayViewportRect_ = false;
};
