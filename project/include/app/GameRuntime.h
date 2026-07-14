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
#include <unordered_set>
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

    static bool PreloadSharedResourceStep(DirectXCommon* dxCommon, SrvManager* srvManager);
    static bool AreSharedResourcesPreloaded();
    static int GetSharedResourcePreloadStep();
    static int GetSharedResourcePreloadStepCount();
    static const char* GetSharedResourcePreloadLabel();
    static float GetSharedResourceLastStepMs();
    static float GetSharedResourceTotalMs();

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
            bool additive = true;
        };
        static constexpr size_t kMaxVisuals = 16;
        std::array<Visual, kMaxVisuals> visuals{};
        size_t visualCount = 0;
    };

    struct PlayerDodgeAfterimage {
        std::unique_ptr<Object3d> object;
        Math::Vector3 position{};
        int direction = 1;
        float age = 0.0f;
        float duration = 16.0f;
        bool isActive = false;
    };

    struct PlayerFlightAura {
        std::unique_ptr<Object3d> object;
        Model* model = nullptr;
        Math::Vector3 offset{};
        Math::Vector4 color{ 0.70f, 0.95f, 1.0f, 0.16f };
        float baseSize = 1.0f;
        float aspectX = 1.0f;
        float aspectY = 1.0f;
        float pulseOffset = 0.0f;
        float roll = 0.0f;
        float rollSpeed = 0.0f;
    };

    struct PlayerExhaustParticle {
        std::unique_ptr<Object3d> object;
        Model* model = nullptr;
        Math::Vector3 position{};
        Math::Vector3 velocity{};
        Math::Vector4 color{ 0.35f, 0.82f, 1.0f, 0.0f };
        float age = 0.0f;
        float lifetime = 0.24f;
        float startSize = 0.24f;
        float endSize = 0.04f;
        float aspectX = 1.0f;
        float aspectY = 1.0f;
        float roll = 0.0f;
        float rollSpeed = 0.0f;
        bool isActive = false;
    };

    struct ContactShadow {
        std::unique_ptr<Object3d> object;
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
        float curveInfluence = 0.0f;
        float phase = 0.0f;
        float currentLocalZ = 0.0f;
        float drawFarLocalZ = 300.0f;
        bool isVisible = true;
        bool billboard = false;
    };

    struct DepthCueEffect {
        std::unique_ptr<Object3d> object;
        Model* model = nullptr;
        Math::Vector3 anchor{};
        Math::Vector4 color{ 1.0f, 1.0f, 1.0f, 0.35f };
        float loopLength = 120.0f;
        float speedMultiplier = 1.0f;
        float lateralDrift = 0.0f;
        float verticalDrift = 0.0f;
        float driftSpeed = 0.02f;
        float phase = 0.0f;
        float baseScale = 0.18f;
        float aspectX = 1.0f;
        float aspectY = 1.0f;
        float spinSpeed = 0.0f;
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
    void InitializeDepthCueEffects();
    void UpdateDepthCueEffects();
    void DrawDepthCueEffects();
    void InitializeRailScenery();
    void UpdateRailScenery();
    void RenderShadowMap();
    void DrawRailScenery();
    void InitializeContactShadows();
    void DrawContactShadows();
    void DrawContactShadowObject(
        Object3d& object,
        const Math::Vector3& position,
        float scaleX,
        float scaleZ,
        float alpha,
        float yaw);
    void UpdateStageDirector();
    void SpawnStageEnemy(
        float x,
        float y,
        float leadDistance,
        Enemy::Behavior behavior,
        Enemy::EntryStyle entryStyle,
        int maxHpOverride = 0,
        float scaleMultiplier = 1.0f);
    Model* GetEnemyModelForBehavior(Enemy::Behavior behavior) const;
    const char* GetEnemyTextureOverrideForBehavior(Enemy::Behavior behavior) const;
    void SpawnBossEnemy();
    void UpdateStageEnemyEvents();
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
    void AddEnemyMuzzleFlashEffect(const Math::Vector3& worldPosition);
    void AddMuzzleFlashEffect(
        const Math::Vector3& worldPosition,
        bool isCharged);
    void AddRewardHeartCollectEffect(const Math::Vector3& worldPosition);
    void AddPlayerDamageEffect(const Math::Vector3& worldPosition);
    void AddPlayerDodgeGrazeEffect(const Math::Vector3& worldPosition);
    void TriggerJustDodge(Bullet& bullet, const Math::Vector3& worldPosition);
    void TriggerPlayerImpactMoment(
        bool isCharged,
        bool isBossHit,
        bool isDestroyed);
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
        const Math::Vector3& velocity,
        bool additive = true);
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
    void InitializePlayerDodgeAfterimages();
    void SpawnPlayerDodgeAfterimage();
    void UpdatePlayerDodgeAfterimages();
    void DrawPlayerDodgeAfterimages();
    void InitializePlayerFlightAura();
    void InitializePlayerExhaustParticles();
    void EmitPlayerExhaustParticles(const Math::Vector3& playerPosition,
        const Math::Vector3& playerRotate);
    void UpdateAndDrawPlayerExhaustParticles();
    void DrawPlayerFlightAura();
    void DrawEditorOverlayGuiRich();
    void DrawHud();
    void DrawBossHud();
    void DrawStageCueHud();
    void DrawLockOnHud();
    void DrawResultOverlay();
    void DrawPerformanceOverlay();
    void DrawBulletEffectObjects();
    void DrawHitEffectObjects();
    void GetEffectiveHudViewportRect(Math::Vector2& min, Math::Vector2& size) const;
    void UpdatePlayerBullets();
    void UpdateEnemyBullets();
    void UpdateEnemies();
    float GetCinematicWorldTimeScale() const;
    Math::Vector3 CalculateAimDirection(const Math::Vector3& origin) const;
    const Enemy* FindHomingTargetForBullet(const Bullet& bullet) const;
    int GetTotalEnemyTargetCount() const;
    int GetRequiredEnemyDefeatsForClear() const;
    const Enemy* GetBossEnemy() const;
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
    std::unordered_set<Bullet*> justDodgedEnemyBullets_;
    std::vector<HitEffect> hitEffects_;
    std::vector<std::unique_ptr<Object3d>> hitEffectObjectPool_;
    std::vector<RailSceneryObject> railSceneryObjects_;
    std::vector<DepthCueEffect> depthCueEffects_;
    std::vector<RewardHeart> rewardHearts_;
    std::array<PlayerDodgeAfterimage, 16> playerDodgeAfterimages_;
    std::array<PlayerFlightAura, 6> playerFlightAuras_;
    std::array<PlayerExhaustParticle, 64> playerExhaustParticles_;
    std::array<ContactShadow, 32> contactShadows_;
    std::array<bool, 5> stageRailEventTriggered_{};
    std::array<bool, 24> stageEnemyEventTriggered_{};
    std::array<WaveTuning, 3> waveTuning_{ {
        { 6, 42, 30.0f },
        { 8, 38, 34.0f },
        { 10, 34, 38.0f }
    } };

    Model* playerModel_ = nullptr;
    Model* bulletModel_ = nullptr;
    Model* enemyModel_ = nullptr;
    Model* enemyFormationModel_ = nullptr;
    Model* enemySwoopModel_ = nullptr;
    Model* enemyShooterModel_ = nullptr;
    Model* enemyHeavyModel_ = nullptr;
    Model* bossModel_ = nullptr;
    Model* effectGlowCoreModel_ = nullptr;
    Model* effectGlowRingModel_ = nullptr;
    Model* effectSparkStarModel_ = nullptr;
    Model* effectBulletGlowModel_ = nullptr;
    Model* effectBulletTrailModel_ = nullptr;
    Model* effectPlayerBulletCoreModel_ = nullptr;
    Model* effectPlayerBulletTrailModel_ = nullptr;
    Model* effectPlayerChargeCoreModel_ = nullptr;
    Model* effectPlayerChargeTrailModel_ = nullptr;
    Model* effectEnemyBulletCoreModel_ = nullptr;
    Model* effectEnemyBulletTailModel_ = nullptr;
    Model* effectImpactBurstModel_ = nullptr;
    Model* effectMagicShardModel_ = nullptr;
    Model* effectExplosionFireballModel_ = nullptr;
    Model* effectExplosionSmokeModel_ = nullptr;
    Model* effectExplosionSparksModel_ = nullptr;
    Model* effectContactShadowModel_ = nullptr;

    int shootCooldown_ = 0;
    int shootBufferTimer_ = 0;
    int enemySpawnTimer_ = 0;
    int enemyShotTimer_ = 32;
    int bossWarningTimer_ = 0;
    int bossIntroTimer_ = 0;
    int bossDefeatFlashTimer_ = 0;
    int currentWaveIndex_ = 0;
    int spawnedEnemyCountInWave_ = 0;
    int defeatedEnemyCountInWave_ = 0;
    int spawnSequenceIndex_ = 0;
    int score_ = 0;
    int defeatedEnemyCount_ = 0;
    int resultTransitionTimer_ = -1;
    int chargeTimer_ = 0;
    int chargeFlashTimer_ = 0;
    int playerDodgeAfterimageTimer_ = 0;
    int justDodgeFlashTimer_ = 0;
    int justDodgeSlowTimer_ = 0;
    int playerImpactFlashTimer_ = 0;
    int playerImpactFlashDuration_ = 1;
    int playerImpactSlowTimer_ = 0;
    int playerImpactSlowDuration_ = 1;
    int cameraShakeTimer_ = 0;
    int cameraShakeDuration_ = 1;
    int bulletPoolWarmupTimer_ = 0;
    size_t playerBulletPoolMisses_ = 0;
    size_t enemyBulletPoolMisses_ = 0;
    size_t hitEffectObjectPoolMisses_ = 0;
    size_t rewardHeartPoolMisses_ = 0;
    size_t visibleSceneryCount_ = 0;
    size_t maxActivePlayerBullets_ = 0;
    size_t maxActiveEnemyBullets_ = 0;
    int chargeShotThreshold_ = 88;
    int normalShootCooldown_ = 17;
    int chargedShootCooldown_ = 30;
    int enemyShotInterval_ = 64;
    int waveStartDelay_ = 90;
    float cameraTimer_ = 0.0f;
    float cameraShakePower_ = 0.0f;
    float railDistance_ = 0.0f;
    float railSpeed_ = 0.115f;
    float targetRailSpeed_ = 0.115f;
    float stageCameraYawBias_ = 0.0f;
    float stageCameraRollBias_ = 0.0f;
    float stageCameraLiftBias_ = 0.0f;
    float stageCameraFovBoost_ = 0.0f;
    float playerExhaustThrust_ = 0.0f;
    float playerExhaustParticleTimer_ = 0.0f;
    float playerImpactSlowScale_ = 1.0f;
    float playerBulletSpeed_ = 1.36f;
    float lockBulletSpeed_ = 1.62f;
    float chargedBulletSpeedMultiplier_ = 1.12f;
    float enemyBulletSpeed_ = 0.36f;
    float lockRadius_ = 118.0f;
    float cameraFovY_ = 0.590f;
    Math::Vector2 hudViewportMin_{ 0.0f, 0.0f };
    Math::Vector2 hudViewportSize_{ 0.0f, 0.0f };
    Math::Vector2 editorOverlayViewportMin_{ 0.0f, 0.0f };
    Math::Vector2 editorOverlayViewportSize_{ 0.0f, 0.0f };
    size_t nextPlayerDodgeAfterimageIndex_ = 0;
    size_t nextPlayerExhaustParticleIndex_ = 0;
    Math::Vector3 cameraTranslate_{ 0.0f, 2.65f, -15.8f };
    Math::Vector3 cameraRotate_{ 0.18f, 0.0f, 0.0f };
    Math::Vector3 previousPlayerTranslate_{ 0.0f, 0.0f, 0.0f };
    const Enemy* lockedEnemy_ = nullptr;
    const char* stageSectionName_ = "Opening";
    const char* stageCombatBeatName_ = "Intro";
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
    bool isPerformanceOverlayVisible_ = false;
    bool isPostEffectBypassEnabled_ = false;
    bool isExitRequested_ = false;
    bool showSkybox_ = true;
    bool bossSpawned_ = false;
    bool bossDefeated_ = false;
    bool bossWarningTriggered_ = false;
    bool wasPlayerDodging_ = false;
    bool isHudViewportRectEnabled_ = false;
    bool hasEditorOverlayViewportRect_ = false;
};
