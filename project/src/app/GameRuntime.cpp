#include "app/GameRuntime.h"

#include "engine/3d/ModelManager.h"
#include "engine/3d/Skybox.h"
#include "engine/3d/TextureManager.h"
#include "engine/io/Input.h"
#include "engine/scene/SceneSerializer.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace {

constexpr const char* kGameSceneFilePath = "resources/game_scene.json";

struct WaveDefinition {
    int enemyCount;
    int spawnInterval;
    float spawnLeadDistance;
};

constexpr WaveDefinition kWaveDefinitions[] = {
    { 6, 80, 26.0f },
    { 8, 60, 30.0f },
    { 10, 45, 34.0f }
};

constexpr int kWaveCount =
    static_cast<int>(sizeof(kWaveDefinitions) / sizeof(kWaveDefinitions[0]));

constexpr int kChargeShotThreshold = 70;
constexpr int kChargeShotMax = 100;

constexpr const char* kRuntimeSceneModelItems[] = {
    "primitive_plane",
    "primitive_ring",
    "primitive_cylinder",
    "primitive_sphere",
    "primitive_triangle",
    "primitive_circle",
    "primitive_box",
    "primitive_torus",
    "primitive_cone",
    "AnimatedCube/AnimatedCube.gltf",
    "simpleSkin/simpleSkin.gltf",
    "human/sneakWalk.gltf",
    "human/walk.gltf"
};

float DistanceSquared(const Math::Vector3& a, const Math::Vector3& b)
{
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    const float z = a.z - b.z;
    return x * x + y * y + z * z;
}

float Lerp(float start, float end, float rate)
{
    return start + (end - start) * rate;
}

Math::Vector3 Lerp(const Math::Vector3& start, const Math::Vector3& end, float rate)
{
    return {
        Lerp(start.x, end.x, rate),
        Lerp(start.y, end.y, rate),
        Lerp(start.z, end.z, rate),
    };
}

int GetTotalEnemyCount()
{
    int total = 0;
    for (const WaveDefinition& wave : kWaveDefinitions) {
        total += wave.enemyCount;
    }
    return total;
}

} // namespace

GameRuntime::GameRuntime() = default;
GameRuntime::~GameRuntime() = default;

void GameRuntime::SetSystems(
    DirectXCommon* dxCommon,
    SrvManager* srvManager,
    SpriteCommon* spriteCommon,
    ImGuiManager* imguiManager,
    Input* input)
{
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    spriteCommon_ = spriteCommon;
    imguiManager_ = imguiManager;
    input_ = input;
}

void GameRuntime::Initialize()
{
    isExitRequested_ = false;

    const std::string environmentTexturePath =
        "resources/skybox/kloofendal_48d_partly_cloudy_puresky_4k_cube.dds";

    object3dCommon_ = std::make_unique<Object3dCommon>();
    object3dCommon_->Initialize(dxCommon_, srvManager_);
    object3dCommon_->SetEnvironmentTexturePath(environmentTexturePath);

    ModelManager::GetInstance()->SetEnvironmentTexturePath(environmentTexturePath);
    TextureManager::GetInstance()->LoadTexture(environmentTexturePath);
    ModelManager::GetInstance()->CreateBox(
        "game_player",
        1.0f,
        0.45f,
        1.3f,
        "resources/uvChecker.png");
    ModelManager::GetInstance()->CreateSphere(
        "game_bullet",
        12,
        24,
        0.35f,
        "resources/gradationLine.png");
    ModelManager::GetInstance()->CreateSphere(
        "game_enemy",
        16,
        32,
        0.9f,
        "resources/uvChecker.png");
    ModelManager::GetInstance()->CreatePlane(
        "primitive_plane",
        8.0f,
        8.0f,
        "resources/checkerBoard.png");
    ModelManager::GetInstance()->CreateTriangle(
        "primitive_triangle",
        1.6f,
        1.6f,
        "resources/uvChecker.png");
    ModelManager::GetInstance()->CreateCircle(
        "primitive_circle",
        32,
        0.9f,
        "resources/uvChecker.png");
    ModelManager::GetInstance()->CreateRing(
        "primitive_ring",
        32,
        2.0f,
        1.0f,
        "resources/gradationLine.png");
    ModelManager::GetInstance()->CreateSphere(
        "primitive_sphere",
        16,
        32,
        1.0f,
        "resources/uvChecker.png");
    ModelManager::GetInstance()->CreateTorus(
        "primitive_torus",
        32,
        16,
        0.8f,
        0.3f,
        "resources/uvChecker.png");
    ModelManager::GetInstance()->CreateCylinder(
        "primitive_cylinder",
        32,
        1.2f,
        1.2f,
        2.5f,
        "resources/gradationLine.png");
    ModelManager::GetInstance()->CreateCone(
        "primitive_cone",
        32,
        0.8f,
        1.6f,
        "resources/uvChecker.png");
    ModelManager::GetInstance()->CreateBox(
        "primitive_box",
        1.5f,
        1.5f,
        1.5f,
        "resources/uvChecker.png");
    ModelManager::GetInstance()->LoadModel("AnimatedCube/AnimatedCube.gltf");
    ModelManager::GetInstance()->LoadModel("simpleSkin/simpleSkin.gltf");
    ModelManager::GetInstance()->LoadModel("human/sneakWalk.gltf");
    ModelManager::GetInstance()->LoadModel("human/walk.gltf");

    playerModel_ = ModelManager::GetInstance()->FindModel("game_player");
    bulletModel_ = ModelManager::GetInstance()->FindModel("game_bullet");
    enemyModel_ = ModelManager::GetInstance()->FindModel("game_enemy");

    camera_ = std::make_unique<Camera>();
    camera_->SetRotate({ 0.15f, 0.0f, 0.0f });
    camera_->SetTranslate({ 0.0f, 2.5f, -13.0f });
    camera_->SetFovY(0.5f);
    camera_->Update();
    object3dCommon_->SetDefaultCamera(camera_.get());

    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(dxCommon_, srvManager_, environmentTexturePath);
    skybox_->Update(camera_.get());

    player_ = std::make_unique<Player>();
    player_->Initialize(object3dCommon_.get(), playerModel_);
    player_->SetRailZ(railDistance_);
    previousPlayerTranslate_ = player_->GetTranslate();
    cameraTranslate_ = camera_->GetTranslate();

    LoadSceneObjects(kGameSceneFilePath);
    InitializeEnvironmentBonusCoins();
}

void GameRuntime::Finalize()
{
    sceneObjects_.clear();
    environmentBonusCoins_.clear();
    playerBullets_.clear();
    enemyBullets_.clear();
    enemies_.clear();
    explosionEffects_.clear();
    player_.reset();
    camera_.reset();
    skybox_.reset();
    object3dCommon_.reset();
}

void GameRuntime::Update()
{
    if (input_ && input_->TriggerKey(DIK_F2)) {
        isExitRequested_ = true;
        return;
    }
    if (input_ && input_->TriggerKey(DIK_F4)) {
        isDebugGuiVisible_ = !isDebugGuiVisible_;
    }

    if (!isGameOver_ && !isGameClear_) {
        railDistance_ += railSpeed_;
    }

    player_->Update(input_);
    player_->SetRailZ(railDistance_);
    UpdateGameCamera();
    if (skybox_) {
        skybox_->Update(camera_.get());
    }
    UpdateLockOnTarget();

    if (!isGameOver_ && !isGameClear_) {
        if (shootCooldown_ > 0) {
            --shootCooldown_;
        }
        const bool isShootPressed = input_ && input_->PushKey(DIK_SPACE);
        if (isShootPressed && shootCooldown_ <= 0) {
            const bool isCharged = chargeTimer_ >= kChargeShotThreshold;
            FirePlayerBullet();
            AddCameraShake(isCharged ? 0.085f : 0.035f, isCharged ? 14 : 8);
            shootCooldown_ = isCharged ? 14 : 8;
        } else if (!isShootPressed) {
            chargeTimer_ =
                (std::min)(chargeTimer_ + 1, kChargeShotMax);
        }
        if (chargeFlashTimer_ > 0) {
            --chargeFlashTimer_;
        }

        UpdateEnemyWave();

        --enemyShotTimer_;
        if (enemyShotTimer_ <= 0) {
            for (const auto& enemy : enemies_) {
                if (enemy->CanShoot()) {
                    FireEnemyBullet(enemy->GetTranslate());
                }
            }
            enemyShotTimer_ = 75;
        }
    }

    UpdatePlayerBullets();
    UpdateEnemyBullets();
    UpdateEnemies();
    UpdateExplosionEffects();
    UpdateEnvironmentBonusCoins();
    CheckBulletBonusCoinCollisions();
    CheckPlayerBonusCoinCollisions();
    CheckBulletEnemyCollisions();
    CheckEnemyBulletPlayerCollisions();
    AdvanceEnemyWaveIfCleared();
    UpdateLockOnTarget();
    DrawHud();
    DrawResultOverlay();

    if (resultTransitionTimer_ > 0) {
        --resultTransitionTimer_;
    }

    for (const auto& sceneObject : sceneObjects_) {
        sceneObject->Update();
    }

    if (isDebugGuiAllowed_ && isDebugGuiVisible_) {
        ImGui::Begin("ゲームデバッグ");
        ImGui::TextUnformatted("F4: ゲームデバッグ表示切り替え");
        ImGui::Text("プレイヤーHP: %d", player_->GetHp());
        ImGui::Text("スコア: %d", score_);
        ImGui::Text("自弾: %zu", playerBullets_.size());
        ImGui::Text("敵弾: %zu", enemyBullets_.size());
        ImGui::Text("敵数: %zu", enemies_.size());
        ImGui::Text(
            "ウェーブ: %d / %d",
            (std::min)(currentWaveIndex_ + 1, kWaveCount),
            kWaveCount);
        ImGui::Text(
            "ウェーブ出現数: %d / %d",
            currentWaveIndex_ < kWaveCount ? spawnedEnemyCountInWave_ : 0,
            currentWaveIndex_ < kWaveCount ?
                kWaveDefinitions[currentWaveIndex_].enemyCount :
                0);
        ImGui::Text(
            "撃破数: %d / %d",
            defeatedEnemyCount_,
            GetTotalEnemyCount());
        ImGui::Text("レール距離: %.1f", railDistance_);
        if (isGameClear_) {
            ImGui::TextUnformatted("ゲームクリア");
        }
        if (isGameOver_) {
            ImGui::TextUnformatted("ゲームオーバー - グレースケール演出");
        }
        ImGui::End();
    }
}

void GameRuntime::SetDebugGuiAllowed(bool isAllowed)
{
    isDebugGuiAllowed_ = isAllowed;
}

void GameRuntime::SetRenderingOptions(bool showSkybox, int postEffectMode)
{
    showSkybox_ = showSkybox;
    postEffectMode_ = postEffectMode;
}

int GameRuntime::GetPlayerHp() const
{
    return player_ ? player_->GetHp() : 0;
}

void GameRuntime::InitializeEnvironmentBonusCoins()
{
    environmentBonusCoins_.clear();

    Model* coinModel =
        ModelManager::GetInstance()->FindModel("primitive_sphere");
    if (!coinModel) {
        return;
    }

    struct SpawnData {
        Math::Vector3 position;
        float phase;
    };
    constexpr SpawnData kSpawnData[] = {
        { { -3.2f, 1.35f, 18.0f }, 0.0f },
        { {  3.1f, 0.65f, 27.0f }, 1.4f },
        { { -1.2f, 1.85f, 38.0f }, 2.2f },
        { {  2.6f, 1.15f, 50.0f }, 3.1f },
        { {  0.0f, 2.1f, 63.0f }, 4.0f },
    };

    environmentBonusCoins_.reserve(
        sizeof(kSpawnData) / sizeof(kSpawnData[0]));

    for (const SpawnData& spawn : kSpawnData) {
        EnvironmentBonusCoin coin{};
        coin.object = std::make_unique<Object3d>();
        coin.object->Initialize(object3dCommon_.get());
        coin.object->SetModel(coinModel);
        coin.object->SetTranslate(spawn.position);
        coin.object->SetScale({ 0.72f, 0.72f, 0.20f });
        coin.object->SetRotate({ 0.0f, 0.0f, 0.0f });
        coin.object->SetTextureFilePath("resources/human/white.png");
        coin.object->SetColor({ 1.0f, 0.78f, 0.18f, 1.0f });
        coin.object->SetLightingMode(2);
        coin.object->SetEnvironmentCoefficient(0.82f);
        coin.object->Update();
        coin.basePosition = spawn.position;
        coin.collisionRadius = 0.85f;
        coin.rotationSpeed = 0.045f + spawn.phase * 0.003f;
        coin.floatPhase = spawn.phase;
        environmentBonusCoins_.push_back(std::move(coin));
    }
}

void GameRuntime::UpdateEnvironmentBonusCoins()
{
    for (EnvironmentBonusCoin& coin : environmentBonusCoins_) {
        if (!coin.isActive || !coin.object) {
            continue;
        }

        coin.floatPhase += 0.035f;
        Math::Vector3 position = coin.basePosition;
        position.y += std::sin(coin.floatPhase) * 0.28f;

        Math::Vector3 rotate = coin.object->GetRotate();
        rotate.y += coin.rotationSpeed;
        rotate.x = std::sin(coin.floatPhase * 0.75f) * 0.12f;
        rotate.z += coin.rotationSpeed * 0.18f;

        coin.object->SetTranslate(position);
        coin.object->SetRotate(rotate);
        coin.object->Update();
    }
}

int GameRuntime::GetPostEffectMode() const
{
    if (postEffectMode_ != 12) {
        return postEffectMode_;
    }
    if (isGameOver_) {
        return 15;
    }
    if (isGameClear_) {
        return 14;
    }
    if (GetPlayerHp() <= 35) {
        return 13;
    }
    return 12;
}

void GameRuntime::LoadSceneObjects(const char* path)
{
    sceneObjects_.clear();

    std::vector<SceneSerializer::ObjectRecord> records;
    SceneSerializer::SceneSettings settings{};
    if (!SceneSerializer::LoadScene(path, records, settings)) {
        return;
    }

    const int modelItemCount =
        static_cast<int>(
            sizeof(kRuntimeSceneModelItems) / sizeof(kRuntimeSceneModelItems[0]));
    for (const SceneSerializer::ObjectRecord& record : records) {
        if (record.modelIndex < 0 || modelItemCount <= record.modelIndex) {
            continue;
        }

        auto sceneObject = std::make_unique<Object3d>();
        sceneObject->Initialize(object3dCommon_.get());
        sceneObject->SetModel(kRuntimeSceneModelItems[record.modelIndex]);
        sceneObject->SetTranslate(record.translate);
        sceneObject->SetRotate(record.rotate);
        sceneObject->SetScale(record.scale);
        sceneObject->SetColor(record.color);
        sceneObject->SetAlphaReference(record.alphaReference);
        sceneObject->SetLightingMode(record.lightingMode);
        if (!record.textureFilePath.empty()) {
            sceneObject->SetTextureFilePath(record.textureFilePath);
        }
        sceneObject->SetEnvironmentCoefficient(0.0f);
        sceneObject->Update();
        sceneObjects_.push_back(std::move(sceneObject));
    }
}

void GameRuntime::Draw()
{
    if (showSkybox_ && skybox_) {
        skybox_->Draw();
    }

    object3dCommon_->CommonDrawSetting();
    for (const auto& sceneObject : sceneObjects_) {
        sceneObject->Draw();
    }
    for (const EnvironmentBonusCoin& coin : environmentBonusCoins_) {
        if (coin.isActive && coin.object) {
            coin.object->Draw();
        }
    }
    if (player_) {
        player_->Draw();
    }
    for (const auto& bullet : playerBullets_) {
        bullet->Draw();
    }
    for (const auto& bullet : enemyBullets_) {
        bullet->Draw();
    }
    for (const auto& enemy : enemies_) {
        enemy->Draw();
    }
}

void GameRuntime::FirePlayerBullet()
{
    if (!player_ || !bulletModel_) {
        return;
    }

    Math::Vector3 spawnPosition = player_->GetTranslate();
    spawnPosition.z += 1.2f;
    Math::Vector3 velocity{ 0.0f, 0.0f, 0.65f };
    Math::Vector4 color{ 1.0f, 0.85f, 0.25f, 1.0f };
    Math::Vector3 scale{ 0.34f, 0.34f, 0.95f };
    float collisionRadius = 0.46f;
    int lifeTimer = 180;
    int hitLimit = 1;
    const bool isCharged = chargeTimer_ >= kChargeShotThreshold;

    if (hasLockTarget_ && lockedEnemy_) {
        Math::Vector3 targetPosition = lockedEnemy_->GetTranslate();
        targetPosition.z -= 0.4f;
        velocity = Math::Normalize({
            targetPosition.x - spawnPosition.x,
            targetPosition.y - spawnPosition.y,
            targetPosition.z - spawnPosition.z
        }) * 0.82f;
        color = { 0.35f, 0.95f, 1.0f, 1.0f };
        scale = { 0.48f, 0.48f, 1.25f };
        collisionRadius = 0.62f;
        lifeTimer = 220;
    }
    if (isCharged) {
        velocity = velocity * 1.18f;
        color = hasLockTarget_ ?
            Math::Vector4{ 0.72f, 1.0f, 1.0f, 1.0f } :
            Math::Vector4{ 1.0f, 0.96f, 0.38f, 1.0f };
        scale = hasLockTarget_ ?
            Math::Vector3{ 0.76f, 0.76f, 1.75f } :
            Math::Vector3{ 0.66f, 0.66f, 1.55f };
        collisionRadius = hasLockTarget_ ? 0.95f : 0.82f;
        lifeTimer = 260;
        hitLimit = 4;
    }

    auto bullet = std::make_unique<Bullet>();
    bullet->Initialize(
        object3dCommon_.get(),
        bulletModel_,
        spawnPosition,
        velocity,
        color,
        lifeTimer,
        scale,
        collisionRadius,
        hitLimit);
    playerBullets_.push_back(std::move(bullet));
    chargeTimer_ = 0;
    if (isCharged) {
        chargeFlashTimer_ = 18;
    }
}

void GameRuntime::FireEnemyBullet(const Math::Vector3& position)
{
    if (!bulletModel_) {
        return;
    }

    Math::Vector3 spawnPosition = position;
    spawnPosition.z -= 1.0f;

    auto bullet = std::make_unique<Bullet>();
    bullet->Initialize(
        object3dCommon_.get(),
        bulletModel_,
        spawnPosition,
        { 0.0f, 0.0f, -0.28f },
        { 1.0f, 0.25f, 0.25f, 1.0f },
        240,
        { 0.42f, 0.42f, 0.42f },
        0.55f);
    enemyBullets_.push_back(std::move(bullet));
}

void GameRuntime::SpawnEnemy()
{
    if (!enemyModel_ || currentWaveIndex_ >= kWaveCount) {
        return;
    }

    const WaveDefinition& wave = kWaveDefinitions[currentWaveIndex_];
    const float xPositions[] = { -4.0f, -2.0f, 0.0f, 2.0f, 4.0f };
    const float yPositions[] = { -1.5f, 0.0f, 1.5f };
    const float x = xPositions[spawnSequenceIndex_ % 5];
    const float y = yPositions[(spawnSequenceIndex_ / 5) % 3];
    const Enemy::Behavior behaviors[] = {
        Enemy::Behavior::Formation,
        Enemy::Behavior::Swoop,
        Enemy::Behavior::StrafeShooter
    };
    const Enemy::Behavior behavior =
        behaviors[spawnSequenceIndex_ % (sizeof(behaviors) / sizeof(behaviors[0]))];
    ++spawnSequenceIndex_;
    ++spawnedEnemyCountInWave_;

    auto enemy = std::make_unique<Enemy>();
    enemy->Initialize(
        object3dCommon_.get(),
        enemyModel_,
        { x, y, railDistance_ + wave.spawnLeadDistance },
        behavior);
    enemies_.push_back(std::move(enemy));
}

void GameRuntime::UpdateEnemyWave()
{
    if (currentWaveIndex_ >= kWaveCount) {
        return;
    }

    const WaveDefinition& wave = kWaveDefinitions[currentWaveIndex_];
    if (spawnedEnemyCountInWave_ >= wave.enemyCount) {
        return;
    }

    if (enemySpawnTimer_ > 0) {
        --enemySpawnTimer_;
        return;
    }

    SpawnEnemy();
    enemySpawnTimer_ = wave.spawnInterval;
}

void GameRuntime::AdvanceEnemyWaveIfCleared()
{
    if (isGameOver_ || isGameClear_ || currentWaveIndex_ >= kWaveCount) {
        return;
    }

    const WaveDefinition& wave = kWaveDefinitions[currentWaveIndex_];
    if (spawnedEnemyCountInWave_ < wave.enemyCount || !enemies_.empty()) {
        return;
    }

    ++currentWaveIndex_;
    spawnedEnemyCountInWave_ = 0;
    enemySpawnTimer_ = 90;

    if (currentWaveIndex_ >= kWaveCount) {
        isGameClear_ = true;
        resultTransitionTimer_ = 90;
    }
}

void GameRuntime::UpdateLockOnTarget()
{
    lockedEnemy_ = nullptr;
    hasLockTarget_ = false;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (input_) {
        const Math::Vector2& mousePosition = input_->GetMousePosition();
        reticleScreen_ = {
            viewport->Pos.x + std::clamp(mousePosition.x, 0.0f, viewport->Size.x),
            viewport->Pos.y + std::clamp(mousePosition.y, 0.0f, viewport->Size.y)
        };
    } else {
        reticleScreen_ = {
            viewport->Pos.x + viewport->Size.x * 0.5f,
            viewport->Pos.y + viewport->Size.y * 0.5f
        };
    }

    if (!camera_ || isGameOver_ || isGameClear_) {
        return;
    }

    constexpr float kLockRadius = 118.0f;
    constexpr float kLockRadiusSq = kLockRadius * kLockRadius;
    float bestScore = kLockRadiusSq;

    for (const auto& enemy : enemies_) {
        if (!enemy || enemy->IsDead()) {
            continue;
        }

        Math::Vector2 screenPosition{};
        if (!TryProjectToScreen(enemy->GetTranslate(), screenPosition)) {
            continue;
        }

        const float dx = screenPosition.x - reticleScreen_.x;
        const float dy = screenPosition.y - reticleScreen_.y;
        const float distanceSq = dx * dx + dy * dy;
        if (distanceSq < bestScore) {
            bestScore = distanceSq;
            lockedEnemy_ = enemy.get();
            lockedEnemyScreen_ = screenPosition;
            hasLockTarget_ = true;
        }
    }
}

bool GameRuntime::TryProjectToScreen(
    const Math::Vector3& worldPosition,
    Math::Vector2& screenPosition) const
{
    if (!camera_) {
        return false;
    }

    const Math::Matrix4x4& viewProjection = camera_->GetViewProjectionMatrix();
    const float clipX =
        worldPosition.x * viewProjection.m[0][0] +
        worldPosition.y * viewProjection.m[1][0] +
        worldPosition.z * viewProjection.m[2][0] +
        viewProjection.m[3][0];
    const float clipY =
        worldPosition.x * viewProjection.m[0][1] +
        worldPosition.y * viewProjection.m[1][1] +
        worldPosition.z * viewProjection.m[2][1] +
        viewProjection.m[3][1];
    const float clipW =
        worldPosition.x * viewProjection.m[0][3] +
        worldPosition.y * viewProjection.m[1][3] +
        worldPosition.z * viewProjection.m[2][3] +
        viewProjection.m[3][3];

    if (clipW <= 0.001f) {
        return false;
    }

    const float ndcX = clipX / clipW;
    const float ndcY = clipY / clipW;
    if (ndcX < -1.2f || 1.2f < ndcX || ndcY < -1.2f || 1.2f < ndcY) {
        return false;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    screenPosition = {
        viewport->Pos.x + (ndcX + 1.0f) * 0.5f * viewport->Size.x,
        viewport->Pos.y + (1.0f - ndcY) * 0.5f * viewport->Size.y
    };
    return true;
}

void GameRuntime::AddExplosionEffect(
    const Math::Vector3& worldPosition,
    float strength)
{
    ExplosionEffect effect{};
    effect.worldPosition = worldPosition;
    effect.duration = 26;
    effect.strength = strength;
    explosionEffects_.push_back(effect);
}

void GameRuntime::AddCoinCollectEffect(const Math::Vector3& worldPosition)
{
    ExplosionEffect effect{};
    effect.worldPosition = worldPosition;
    effect.duration = 34;
    effect.strength = 1.0f;
    effect.isCoinCollect = true;
    explosionEffects_.push_back(effect);
}

void GameRuntime::UpdateExplosionEffects()
{
    for (ExplosionEffect& effect : explosionEffects_) {
        ++effect.age;
    }

    explosionEffects_.erase(
        std::remove_if(
            explosionEffects_.begin(),
            explosionEffects_.end(),
            [](const ExplosionEffect& effect) {
                return effect.age >= effect.duration;
            }),
        explosionEffects_.end());
}

void GameRuntime::DrawExplosionEffects()
{
    if (explosionEffects_.empty()) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    for (const ExplosionEffect& effect : explosionEffects_) {
        Math::Vector2 screenPosition{};
        if (!TryProjectToScreen(effect.worldPosition, screenPosition)) {
            continue;
        }

        const float rate =
            static_cast<float>(effect.age) /
            static_cast<float>((std::max)(effect.duration, 1));
        const float fade = 1.0f - rate;
        const float radius = (18.0f + 58.0f * rate) * effect.strength;
        const int alphaStrong =
            static_cast<int>((std::clamp)(fade, 0.0f, 1.0f) * 230.0f);
        const int alphaSoft =
            static_cast<int>((std::clamp)(fade, 0.0f, 1.0f) * 120.0f);
        const ImVec2 center(screenPosition.x, screenPosition.y);

        if (effect.isCoinCollect) {
            const float pop = std::sin((std::min)(rate, 0.5f) * 3.14159265f);
            const float coinRadius = (10.0f + 24.0f * rate) * effect.strength;
            const int goldAlpha =
                static_cast<int>((std::clamp)(fade, 0.0f, 1.0f) * 210.0f);
            const ImVec2 scorePosition(
                center.x - 18.0f,
                center.y - 36.0f - 28.0f * rate);

            drawList->AddCircleFilled(
                center,
                (6.0f + 7.0f * pop) * effect.strength,
                IM_COL32(255, 225, 96, alphaSoft),
                24);
            drawList->AddCircle(
                center,
                coinRadius,
                IM_COL32(255, 211, 72, goldAlpha),
                36,
                2.0f);
            drawList->AddCircle(
                center,
                coinRadius * 0.54f,
                IM_COL32(255, 246, 170, goldAlpha),
                30,
                1.5f);

            constexpr int kSparkCount = 6;
            for (int index = 0; index < kSparkCount; ++index) {
                const float angle =
                    (static_cast<float>(index) / static_cast<float>(kSparkCount)) *
                    6.2831853f +
                    0.35f;
                const float inner = coinRadius * 0.35f;
                const float outer = coinRadius * (0.92f + 0.18f * pop);
                const ImVec2 start(
                    center.x + std::cos(angle) * inner,
                    center.y + std::sin(angle) * inner);
                const ImVec2 end(
                    center.x + std::cos(angle) * outer,
                    center.y + std::sin(angle) * outer);
                drawList->AddLine(
                    start,
                    end,
                    IM_COL32(255, 236, 132, goldAlpha),
                    2.0f);
            }

            drawList->AddText(
                scorePosition,
                IM_COL32(255, 235, 120, goldAlpha),
                "+250");
            continue;
        }

        drawList->AddCircleFilled(
            center,
            radius * 0.42f,
            IM_COL32(255, 242, 130, alphaSoft),
            36);
        drawList->AddCircle(
            center,
            radius,
            IM_COL32(255, 176, 76, alphaStrong),
            48,
            3.0f);
        drawList->AddCircle(
            center,
            radius * 0.56f,
            IM_COL32(118, 232, 255, alphaStrong),
            40,
            2.0f);

        constexpr int kSparkCount = 8;
        for (int index = 0; index < kSparkCount; ++index) {
            const float angle =
                (static_cast<float>(index) / static_cast<float>(kSparkCount)) *
                6.2831853f;
            const float inner = radius * 0.34f;
            const float outer = radius * (0.72f + 0.22f * rate);
            const ImVec2 start(
                center.x + std::cos(angle) * inner,
                center.y + std::sin(angle) * inner);
            const ImVec2 end(
                center.x + std::cos(angle) * outer,
                center.y + std::sin(angle) * outer);
            drawList->AddLine(
                start,
                end,
                IM_COL32(255, 246, 176, alphaStrong),
                2.0f);
        }
    }
}

void GameRuntime::DrawHud()
{
    if (!player_) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const ImVec2 origin = ImGui::GetMainViewport()->Pos;
    const ImVec2 panelMin(origin.x + 22.0f, origin.y + 22.0f);
    const ImVec2 panelMax(panelMin.x + 238.0f, panelMin.y + 78.0f);
    const ImVec2 barMin(panelMin.x + 16.0f, panelMin.y + 30.0f);
    const ImVec2 barMax(barMin.x + 190.0f, barMin.y + 12.0f);
    const ImVec2 chargeBarMin(panelMin.x + 16.0f, panelMin.y + 58.0f);
    const ImVec2 chargeBarMax(chargeBarMin.x + 190.0f, chargeBarMin.y + 8.0f);

    const int maxHp = (std::max)(GetPlayerMaxHp(), 1);
    const int hp = (std::clamp)(GetPlayerHp(), 0, maxHp);
    const float hpRate =
        static_cast<float>(hp) / static_cast<float>(maxHp);
    const ImVec2 fillMax(
        barMin.x + (barMax.x - barMin.x) * hpRate,
        barMax.y);

    drawList->AddRectFilled(
        panelMin,
        panelMax,
        IM_COL32(10, 15, 26, 178),
        5.0f);
    drawList->AddRect(
        panelMin,
        panelMax,
        IM_COL32(120, 170, 220, 80),
        5.0f);
    drawList->AddText(
        ImVec2(panelMin.x + 14.0f, panelMin.y + 8.0f),
        IM_COL32(235, 244, 255, 235),
        "HP");
    drawList->AddText(
        ImVec2(panelMin.x + 178.0f, panelMin.y + 8.0f),
        IM_COL32(210, 224, 238, 220),
        (std::to_string(hp) + "/" + std::to_string(maxHp)).c_str());
    drawList->AddRectFilled(
        barMin,
        barMax,
        IM_COL32(28, 36, 52, 230),
        3.0f);
    drawList->AddRectFilled(
        barMin,
        fillMax,
        IM_COL32(84, 230, 142, 245),
        3.0f);
    drawList->AddRect(
        barMin,
        barMax,
        IM_COL32(235, 245, 255, 105),
        3.0f);

    const float chargeRate = (std::clamp)(
        static_cast<float>(chargeTimer_) / static_cast<float>(kChargeShotThreshold),
        0.0f,
        1.0f);
    const bool isChargeReady = chargeTimer_ >= kChargeShotThreshold;
    const ImVec2 chargeFillMax(
        chargeBarMin.x + (chargeBarMax.x - chargeBarMin.x) * chargeRate,
        chargeBarMax.y);
    drawList->AddText(
        ImVec2(panelMin.x + 14.0f, panelMin.y + 44.0f),
        IM_COL32(220, 235, 248, 210),
        "CHG");
    drawList->AddRectFilled(
        chargeBarMin,
        chargeBarMax,
        IM_COL32(26, 34, 48, 215),
        3.0f);
    drawList->AddRectFilled(
        chargeBarMin,
        chargeFillMax,
        isChargeReady ?
            IM_COL32(105, 235, 255, 245) :
            IM_COL32(245, 205, 82, 225),
        3.0f);
    drawList->AddRect(
        chargeBarMin,
        chargeBarMax,
        isChargeReady || chargeFlashTimer_ > 0 ?
            IM_COL32(175, 250, 255, 180) :
            IM_COL32(235, 245, 255, 85),
        3.0f);

    DrawExplosionEffects();
    DrawLockOnHud();
}

void GameRuntime::DrawLockOnHud()
{
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const ImU32 reticleColor =
        hasLockTarget_ ? IM_COL32(98, 235, 255, 245) : IM_COL32(230, 240, 255, 160);
    constexpr float kReticleSize = 18.0f;
    constexpr float kReticleGap = 5.0f;

    drawList->AddCircle(
        ImVec2(reticleScreen_.x, reticleScreen_.y),
        kReticleSize,
        reticleColor,
        40,
        2.0f);
    drawList->AddLine(
        ImVec2(reticleScreen_.x - kReticleSize - kReticleGap, reticleScreen_.y),
        ImVec2(reticleScreen_.x - kReticleGap, reticleScreen_.y),
        reticleColor,
        2.0f);
    drawList->AddLine(
        ImVec2(reticleScreen_.x + kReticleGap, reticleScreen_.y),
        ImVec2(reticleScreen_.x + kReticleSize + kReticleGap, reticleScreen_.y),
        reticleColor,
        2.0f);
    drawList->AddLine(
        ImVec2(reticleScreen_.x, reticleScreen_.y - kReticleSize - kReticleGap),
        ImVec2(reticleScreen_.x, reticleScreen_.y - kReticleGap),
        reticleColor,
        2.0f);
    drawList->AddLine(
        ImVec2(reticleScreen_.x, reticleScreen_.y + kReticleGap),
        ImVec2(reticleScreen_.x, reticleScreen_.y + kReticleSize + kReticleGap),
        reticleColor,
        2.0f);

    if (!hasLockTarget_) {
        return;
    }

    constexpr float kMarkerHalf = 28.0f;
    constexpr float kCorner = 11.0f;
    const ImVec2 center(lockedEnemyScreen_.x, lockedEnemyScreen_.y);
    const ImU32 markerColor = IM_COL32(104, 255, 188, 255);

    drawList->AddCircle(center, kMarkerHalf, markerColor, 48, 2.0f);
    drawList->AddLine(
        ImVec2(center.x - kMarkerHalf, center.y - kMarkerHalf),
        ImVec2(center.x - kMarkerHalf + kCorner, center.y - kMarkerHalf),
        markerColor,
        2.5f);
    drawList->AddLine(
        ImVec2(center.x - kMarkerHalf, center.y - kMarkerHalf),
        ImVec2(center.x - kMarkerHalf, center.y - kMarkerHalf + kCorner),
        markerColor,
        2.5f);
    drawList->AddLine(
        ImVec2(center.x + kMarkerHalf, center.y - kMarkerHalf),
        ImVec2(center.x + kMarkerHalf - kCorner, center.y - kMarkerHalf),
        markerColor,
        2.5f);
    drawList->AddLine(
        ImVec2(center.x + kMarkerHalf, center.y - kMarkerHalf),
        ImVec2(center.x + kMarkerHalf, center.y - kMarkerHalf + kCorner),
        markerColor,
        2.5f);
    drawList->AddLine(
        ImVec2(center.x - kMarkerHalf, center.y + kMarkerHalf),
        ImVec2(center.x - kMarkerHalf + kCorner, center.y + kMarkerHalf),
        markerColor,
        2.5f);
    drawList->AddLine(
        ImVec2(center.x - kMarkerHalf, center.y + kMarkerHalf),
        ImVec2(center.x - kMarkerHalf, center.y + kMarkerHalf - kCorner),
        markerColor,
        2.5f);
    drawList->AddLine(
        ImVec2(center.x + kMarkerHalf, center.y + kMarkerHalf),
        ImVec2(center.x + kMarkerHalf - kCorner, center.y + kMarkerHalf),
        markerColor,
        2.5f);
    drawList->AddLine(
        ImVec2(center.x + kMarkerHalf, center.y + kMarkerHalf),
        ImVec2(center.x + kMarkerHalf, center.y + kMarkerHalf - kCorner),
        markerColor,
        2.5f);
    drawList->AddLine(
        ImVec2(reticleScreen_.x, reticleScreen_.y),
        center,
        IM_COL32(104, 255, 188, 90),
        1.5f);
}

void GameRuntime::DrawResultOverlay()
{
    if (!isGameOver_ && !isGameClear_) {
        return;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const ImVec2 center(
        viewport->Pos.x + viewport->Size.x * 0.5f,
        viewport->Pos.y + viewport->Size.y * 0.44f);
    const ImVec2 panelSize(340.0f, 112.0f);
    const ImVec2 panelMin(
        center.x - panelSize.x * 0.5f,
        center.y - panelSize.y * 0.5f);
    const ImVec2 panelMax(
        center.x + panelSize.x * 0.5f,
        center.y + panelSize.y * 0.5f);

    const char* title = isGameClear_ ? "MISSION CLEAR" : "GAME OVER";
    const char* guide = "F2: 編集モードへ戻る";
    const ImVec2 titleSize = ImGui::CalcTextSize(title);
    const ImVec2 guideSize = ImGui::CalcTextSize(guide);

    drawList->AddRectFilled(
        panelMin,
        panelMax,
        IM_COL32(8, 12, 20, 205),
        8.0f);
    drawList->AddRect(
        panelMin,
        panelMax,
        isGameClear_ ?
            IM_COL32(110, 225, 170, 180) :
            IM_COL32(235, 110, 110, 180),
        8.0f);
    drawList->AddText(
        ImVec2(center.x - titleSize.x * 0.5f, panelMin.y + 28.0f),
        isGameClear_ ?
            IM_COL32(150, 255, 205, 255) :
            IM_COL32(255, 145, 145, 255),
        title);
    drawList->AddText(
        ImVec2(center.x - guideSize.x * 0.5f, panelMin.y + 68.0f),
        IM_COL32(225, 235, 245, 225),
        guide);
}

void GameRuntime::UpdatePlayerBullets()
{
    for (auto iterator = playerBullets_.begin();
        iterator != playerBullets_.end();) {
        (*iterator)->Update();
        if ((*iterator)->IsDead()) {
            iterator = playerBullets_.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

void GameRuntime::UpdateEnemyBullets()
{
    for (auto iterator = enemyBullets_.begin();
        iterator != enemyBullets_.end();) {
        (*iterator)->Update();
        if ((*iterator)->IsDead()) {
            iterator = enemyBullets_.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

void GameRuntime::UpdateEnemies()
{
    for (auto iterator = enemies_.begin(); iterator != enemies_.end();) {
        (*iterator)->Update(railDistance_);
        if ((*iterator)->IsDead()) {
            iterator = enemies_.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

void GameRuntime::CheckBulletBonusCoinCollisions()
{
    for (auto& bullet : playerBullets_) {
        if (bullet->IsDead()) {
            continue;
        }

        for (EnvironmentBonusCoin& coin : environmentBonusCoins_) {
            if (!coin.isActive || !coin.object) {
                continue;
            }

            const float radius = bullet->GetRadius() + coin.collisionRadius;
            if (DistanceSquared(
                bullet->GetTranslate(),
                coin.object->GetTranslate()) <= radius * radius) {
                coin.isActive = false;
                bullet->RegisterHit();
                score_ += 250;
                AddCoinCollectEffect(coin.object->GetTranslate());
                AddCameraShake(0.018f, 5);
                break;
            }
        }
    }
}

void GameRuntime::CheckPlayerBonusCoinCollisions()
{
    if (!player_ || player_->IsDead()) {
        return;
    }

    for (EnvironmentBonusCoin& coin : environmentBonusCoins_) {
        if (!coin.isActive || !coin.object) {
            continue;
        }

        const float radius = player_->GetRadius() + coin.collisionRadius;
        if (DistanceSquared(
            player_->GetTranslate(),
            coin.object->GetTranslate()) <= radius * radius) {
            coin.isActive = false;
            score_ += 250;
            AddCoinCollectEffect(coin.object->GetTranslate());
            AddCameraShake(0.018f, 5);
        }
    }
}

void GameRuntime::CheckBulletEnemyCollisions()
{
    for (auto& bullet : playerBullets_) {
        if (bullet->IsDead()) {
            continue;
        }

        for (auto& enemy : enemies_) {
            if (enemy->IsDead()) {
                continue;
            }

            const float radius = bullet->GetRadius() + enemy->GetRadius();
            if (DistanceSquared(
                bullet->GetTranslate(),
                enemy->GetTranslate()) <= radius * radius) {
                AddExplosionEffect(
                    enemy->GetTranslate(),
                    bullet->GetRadius() >= 0.8f ? 1.28f : 1.0f);
                bullet->RegisterHit();
                enemy->Kill();
                AddCameraShake(0.045f, 8);
                score_ += 100;
                ++defeatedEnemyCount_;
                if (bullet->IsDead()) {
                    break;
                }
            }
        }
    }
}

void GameRuntime::CheckEnemyBulletPlayerCollisions()
{
    if (!player_ || player_->IsDead()) {
        return;
    }

    for (auto& bullet : enemyBullets_) {
        if (bullet->IsDead()) {
            continue;
        }

        const float radius = bullet->GetRadius() + player_->GetRadius();
        if (DistanceSquared(
            bullet->GetTranslate(),
            player_->GetTranslate()) <= radius * radius) {
            bullet->Kill();
            player_->Damage(10);
            AddCameraShake(0.2f, 18);
            if (player_->IsDead() && !isGameOver_) {
                isGameOver_ = true;
                resultTransitionTimer_ = 90;
            }
            break;
        }
    }
}

void GameRuntime::UpdateGameCamera()
{
    if (!camera_ || !player_) {
        return;
    }

    cameraTimer_ += 1.0f;

    const Math::Vector3 playerTranslate = player_->GetTranslate();
    const Math::Vector3 playerVelocity = {
        playerTranslate.x - previousPlayerTranslate_.x,
        playerTranslate.y - previousPlayerTranslate_.y,
        0.0f,
    };
    previousPlayerTranslate_ = playerTranslate;

    float shakeRate = 0.0f;
    if (cameraShakeTimer_ > 0) {
        shakeRate =
            static_cast<float>(cameraShakeTimer_) /
            static_cast<float>((std::max)(cameraShakeDuration_, 1));
        --cameraShakeTimer_;
    } else {
        cameraShakePower_ = 0.0f;
    }

    const float bob = std::sin(cameraTimer_ * 0.045f) * 0.08f;
    const float shakeX = std::sin(cameraTimer_ * 1.9f) * cameraShakePower_ * shakeRate;
    const float shakeY = std::cos(cameraTimer_ * 2.3f) * cameraShakePower_ * shakeRate;
    const float resultZoom =
        isGameClear_ ? 0.9f :
        isGameOver_ ? -0.8f :
        0.0f;

    const Math::Vector3 targetTranslate = {
        playerTranslate.x * 0.22f + playerVelocity.x * 1.1f + shakeX,
        2.55f + playerTranslate.y * 0.15f + playerVelocity.y * 0.8f + bob + shakeY,
        railDistance_ - 13.8f + resultZoom,
    };

    cameraTranslate_ = Lerp(cameraTranslate_, targetTranslate, 0.06f);

    const Math::Vector3 targetRotate = {
        0.18f + playerTranslate.y * 0.006f + bob * 0.01f + shakeY * 0.01f,
        -playerTranslate.x * 0.004f,
        shakeX * 0.006f,
    };

    const float targetFov =
        0.5f +
        (isGameClear_ ? -0.035f : 0.0f) +
        (isGameOver_ ? 0.025f : 0.0f) +
        cameraShakePower_ * shakeRate * 0.04f;

    camera_->SetTranslate(cameraTranslate_);
    camera_->SetRotate(targetRotate);
    camera_->SetFovY(targetFov);
    camera_->Update();
}

void GameRuntime::AddCameraShake(float power, int duration)
{
    cameraShakePower_ = (std::max)(cameraShakePower_, power);
    cameraShakeDuration_ = (std::max)(duration, 1);
    cameraShakeTimer_ = (std::max)(cameraShakeTimer_, cameraShakeDuration_);
}
