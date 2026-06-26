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
#include <unordered_map>
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
    const Math::Matrix4x4& GetProjectionMatrix() const;
    bool IsExitRequested() const { return isExitRequested_; }
    int GetPlayerHp() const;
    int GetPlayerMaxHp() const { return 100; }

private:
    enum class HitEffectType {
        EnemyImpact,
        EnemyDestroy,
        RewardCollect,
        PlayerDamage
    };

    struct HitEffect {
        Math::Vector3 worldPosition{};
        float age = 0.0f;
        int duration = 24;
        float strength = 1.0f;
        int scoreValue = 0;
        HitEffectType type = HitEffectType::EnemyDestroy;
        struct Visual {
            std::unique_ptr<Object3d> object;
            Math::Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
            float baseSize = 1.0f;
            float growth = 1.0f;
            float spin = 0.0f;
            float popDelay = 0.0f;
            float aspectX = 1.0f;
            float aspectY = 1.0f;
            Math::Vector3 velocity{};
        };
        static constexpr size_t kMaxVisuals = 12;
        std::array<Visual, kMaxVisuals> visuals{};
        size_t visualCount = 0;
    };

    struct RewardHeart {
        std::unique_ptr<Object3d> object;
        Math::Vector3 position{};
        Math::Vector3 velocity{};
        float collisionRadius = 0.42f;
        float age = 0.0f;
        float collectDelay = 12.0f;
        float life = 180.0f;
        float phase = 0.0f;
        float baseScale = 0.34f;
        int scoreValue = 25;
        bool isActive = false;
    };

    struct RailSceneryObject {
        std::unique_ptr<Object3d> object;
        Math::Vector3 anchor{};
        Math::Vector3 scale{ 1.0f, 1.0f, 1.0f };
        Math::Vector3 rotate{};
        Math::Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
        float loopLength = 120.0f;
        float speedMultiplier = 1.0f;
        float lateralDrift = 0.0f;
        float verticalDrift = 0.0f;
        float driftSpeed = 0.02f;
        float rollSpeed = 0.0f;
        float phase = 0.0f;
        bool billboard = false;
    };

    struct WaveTuning {
        int enemyCount = 6;
        int spawnInterval = 80;
        float spawnLeadDistance = 26.0f;
    };

    void FirePlayerBullet();
    void FireEnemyBullet(const Math::Vector3& position);
    void SpawnEnemy();
    void InitializeRewardHearts();
    void SpawnRewardHearts(const Math::Vector3& worldPosition, int count);
    void UpdateRewardHearts();
    void InitializeRailScenery();
    void UpdateRailScenery();
    void DrawRailScenery();
    void UpdateEnemyWave();
    void AdvanceEnemyWaveIfCleared();
    void UpdateLockOnTarget();
    bool TryProjectToScreen(
        const Math::Vector3& worldPosition,
        Math::Vector2& screenPosition) const;
    void AddEnemyHitEffect(
        const Math::Vector3& worldPosition,
        float strength = 1.0f);
    void AddEnemyImpactEffect(
        const Math::Vector3& worldPosition,
        float strength = 1.0f);
    void AddMuzzleFlashEffect(
        const Math::Vector3& worldPosition,
        bool isCharged);
    void AddRewardHeartCollectEffect(const Math::Vector3& worldPosition);
    void AddPlayerDamageEffect(const Math::Vector3& worldPosition);
    void AddHitEffectVisual(
        HitEffect& effect,
        Model* model,
        const Math::Vector3& worldPosition,
        const Math::Vector4& color,
        float baseSize,
        float growth,
        float spin,
        float popDelay,
        float aspectX,
        float aspectY,
        const Math::Vector3& velocity);
    void PrewarmHitEffectObjectPool();
    void UpdateHitEffectObjectPoolWarmup();
    std::unique_ptr<Object3d> CreatePooledHitEffectObject();
    std::unique_ptr<Object3d> AcquireHitEffectObject();
    void RecycleHitEffectVisuals(HitEffect& effect);
    bool HandleRuntimeShortcuts();
    void UpdateRailProgress();
    void UpdatePlayerAndCamera();
    void UpdatePlayerShooting();
    void UpdateEnemyActions();
    void UpdateWorldEntities();
    void UpdateGameplayCollisions();
    void UpdateResultAndSceneObjects();
    void UpdateHitEffects();
    void DrawHitEffects();
    void DrawEditorOverlayGuiRich();
    void DrawHud();
    void DrawLockOnHud();
    void DrawResultOverlay();
    void DrawPerformanceOverlay();
    void DrawBulletEffectObjects();
    void DrawHitEffectObjects();
    void GetEffectiveHudViewportRect(Math::Vector2& min, Math::Vector2& size) const;
    void UpdatePlayerBullets();
    void UpdateEnemyBullets();
    void UpdateEnemies();
    Math::Vector3 CalculateAimDirection(const Math::Vector3& origin) const;
    const Enemy* FindHomingTargetForBullet(const Bullet& bullet) const;
    int GetTotalEnemyTargetCount() const;
    void CheckBulletEnemyCollisions();
    void CheckEnemyBulletPlayerCollisions();
    void UpdateGameCamera();
    void AddCameraShake(float power, int duration);
    void PrewarmBulletPools();
    void UpdateBulletPoolWarmup();
    std::unique_ptr<Bullet> CreatePooledPlayerBullet();
    std::unique_ptr<Bullet> CreatePooledEnemyBullet();
    std::unique_ptr<Bullet> AcquireBullet(std::vector<std::unique_ptr<Bullet>>& pool);
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
    std::vector<std::unique_ptr<Bullet>> playerBulletPool_;
    std::vector<std::unique_ptr<Bullet>> enemyBulletPool_;
    std::list<std::unique_ptr<Enemy>> enemies_;
    std::unordered_map<Bullet*, const Enemy*> homingBulletTargets_;
    std::vector<HitEffect> hitEffects_;
    std::vector<std::unique_ptr<Object3d>> hitEffectObjectPool_;
    std::vector<RailSceneryObject> railSceneryObjects_;
    std::vector<RewardHeart> rewardHearts_;
    std::array<WaveTuning, 3> waveTuning_{ {
        { 6, 80, 26.0f },
        { 8, 60, 30.0f },
        { 10, 45, 34.0f }
    } };

    Model* playerModel_ = nullptr;
    Model* bulletModel_ = nullptr;
    Model* enemyModel_ = nullptr;
    Model* effectGlowCoreModel_ = nullptr;
    Model* effectGlowRingModel_ = nullptr;
    Model* effectSparkStarModel_ = nullptr;
    Model* effectBulletGlowModel_ = nullptr;
    Model* effectBulletTrailModel_ = nullptr;
    Model* effectPlayerBulletCoreModel_ = nullptr;
    Model* effectPlayerBulletTrailModel_ = nullptr;
    Model* effectEnemyBulletCoreModel_ = nullptr;
    Model* effectEnemyBulletTailModel_ = nullptr;
    Model* effectImpactBurstModel_ = nullptr;
    Model* effectMagicShardModel_ = nullptr;

    int shootCooldown_ = 0;
    int shootBufferTimer_ = 0;
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
    int bulletPoolWarmupTimer_ = 0;
    size_t playerBulletPoolMisses_ = 0;
    size_t enemyBulletPoolMisses_ = 0;
    size_t hitEffectObjectPoolMisses_ = 0;
    size_t rewardHeartPoolMisses_ = 0;
    size_t maxActivePlayerBullets_ = 0;
    size_t maxActiveEnemyBullets_ = 0;
    int chargeShotThreshold_ = 70;
    int normalShootCooldown_ = 4;
    int chargedShootCooldown_ = 14;
    int enemyShotInterval_ = 75;
    int waveStartDelay_ = 90;
    float cameraTimer_ = 0.0f;
    float cameraShakePower_ = 0.0f;
    float railDistance_ = 0.0f;
    float railSpeed_ = 0.045f;
    float playerBulletSpeed_ = 1.28f;
    float lockBulletSpeed_ = 1.48f;
    float chargedBulletSpeedMultiplier_ = 1.10f;
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
    bool isReticleOnTarget_ = false;
    int postEffectMode_ = 12;
    bool isGameOver_ = false;
    bool isGameClear_ = false;
    bool isEditorOverlayVisible_ = false;
    bool isPerformanceOverlayVisible_ = true;
    bool isPostEffectBypassEnabled_ = false;
    bool isExitRequested_ = false;
    bool showSkybox_ = true;
    bool isHudViewportRectEnabled_ = false;
    bool hasEditorOverlayViewportRect_ = false;
};
