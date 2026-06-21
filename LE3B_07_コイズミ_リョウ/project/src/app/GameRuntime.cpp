#include "app/GameRuntime.h"

#include "engine/3d/ModelManager.h"
#include "engine/3d/Skybox.h"
#include "engine/3d/TextureManager.h"
#include "engine/base/DirectXCommon.h"
#include "engine/io/Input.h"
#include "engine/scene/SceneSerializer.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <IconsFontAwesome6.h>
#include <ImGuiFileDialog.h>
#include <implot.h>
#include <imgui_node_editor.h>
#include <TextEditor.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace {

constexpr const char* kGameSceneFilePath = "resources/game_scene.json";

struct EnemySpawnPattern {
    float x;
    float y;
    Enemy::Behavior behavior;
    Enemy::EntryStyle entryStyle;
};

constexpr int kWaveCount = 3;

constexpr EnemySpawnPattern kWaveOnePatterns[] = {
    { -2.8f, -0.7f, Enemy::Behavior::Formation, Enemy::EntryStyle::VFormation },
    {  0.0f,  0.3f, Enemy::Behavior::Formation, Enemy::EntryStyle::VFormation },
    {  2.8f, -0.7f, Enemy::Behavior::Formation, Enemy::EntryStyle::VFormation },
    { -4.2f,  1.0f, Enemy::Behavior::Swoop, Enemy::EntryStyle::LeftSweep },
    {  4.2f,  1.0f, Enemy::Behavior::Swoop, Enemy::EntryStyle::RightSweep },
    {  0.0f, -1.1f, Enemy::Behavior::StrafeShooter, Enemy::EntryStyle::PopShooter }
};

constexpr EnemySpawnPattern kWaveTwoPatterns[] = {
    { -5.2f,  0.8f, Enemy::Behavior::Swoop, Enemy::EntryStyle::LeftSweep },
    {  5.2f, -0.4f, Enemy::Behavior::Swoop, Enemy::EntryStyle::RightSweep },
    { -2.2f,  1.2f, Enemy::Behavior::StrafeShooter, Enemy::EntryStyle::PopShooter },
    {  2.2f,  1.2f, Enemy::Behavior::StrafeShooter, Enemy::EntryStyle::PopShooter },
    { -3.4f, -1.1f, Enemy::Behavior::Formation, Enemy::EntryStyle::VFormation },
    {  0.0f, -0.2f, Enemy::Behavior::Formation, Enemy::EntryStyle::VFormation },
    {  3.4f, -1.1f, Enemy::Behavior::Formation, Enemy::EntryStyle::VFormation },
    {  0.0f,  1.8f, Enemy::Behavior::Swoop, Enemy::EntryStyle::RightSweep }
};

constexpr EnemySpawnPattern kWaveThreePatterns[] = {
    { -4.6f,  1.4f, Enemy::Behavior::Swoop, Enemy::EntryStyle::LeftSweep },
    {  4.6f,  1.4f, Enemy::Behavior::Swoop, Enemy::EntryStyle::RightSweep },
    { -2.8f,  0.4f, Enemy::Behavior::StrafeShooter, Enemy::EntryStyle::PopShooter },
    {  2.8f,  0.4f, Enemy::Behavior::StrafeShooter, Enemy::EntryStyle::PopShooter },
    {  0.0f, -1.0f, Enemy::Behavior::Formation, Enemy::EntryStyle::VFormation },
    { -3.8f, -0.7f, Enemy::Behavior::Formation, Enemy::EntryStyle::VFormation },
    {  3.8f, -0.7f, Enemy::Behavior::Formation, Enemy::EntryStyle::VFormation },
    { -1.4f,  1.8f, Enemy::Behavior::StrafeShooter, Enemy::EntryStyle::PopShooter },
    {  1.4f,  1.8f, Enemy::Behavior::StrafeShooter, Enemy::EntryStyle::PopShooter },
    {  0.0f,  0.6f, Enemy::Behavior::Swoop, Enemy::EntryStyle::LeftSweep }
};

EnemySpawnPattern GetEnemySpawnPattern(int waveIndex, int sequenceIndex)
{
    if (waveIndex == 0) {
        return kWaveOnePatterns[
            sequenceIndex %
            (sizeof(kWaveOnePatterns) / sizeof(kWaveOnePatterns[0]))];
    }
    if (waveIndex == 1) {
        return kWaveTwoPatterns[
            sequenceIndex %
            (sizeof(kWaveTwoPatterns) / sizeof(kWaveTwoPatterns[0]))];
    }
    return kWaveThreePatterns[
        sequenceIndex %
        (sizeof(kWaveThreePatterns) / sizeof(kWaveThreePatterns[0]))];
}

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

Math::Vector3 TransformCoord(const Math::Vector3& v, const Math::Matrix4x4& m)
{
    const float x =
        v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + m.m[3][0];
    const float y =
        v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + m.m[3][1];
    const float z =
        v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + m.m[3][2];
    const float w =
        v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + m.m[3][3];
    if (std::abs(w) <= 0.0001f) {
        return { x, y, z };
    }
    return { x / w, y / w, z / w };
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
    isGameClear_ = false;
    isGameOver_ = false;
    currentWaveIndex_ = 0;
    spawnedEnemyCountInWave_ = 0;
    defeatedEnemyCountInWave_ = 0;
    spawnSequenceIndex_ = 0;
    defeatedEnemyCount_ = 0;
    score_ = 0;
    enemySpawnTimer_ = 0;
    enemyShotTimer_ = 60;
    resultTransitionTimer_ = -1;
    railDistance_ = 0.0f;
    shootCooldown_ = 0;
    chargeTimer_ = 0;
    chargeFlashTimer_ = 0;
    cameraShakeTimer_ = 0;

    const std::string environmentTexturePath =
        "resources/skybox/kloofendal_48d_partly_cloudy_puresky_4k_cube.dds";

    object3dCommon_ = std::make_unique<Object3dCommon>();
    object3dCommon_->Initialize(dxCommon_, srvManager_);
    object3dCommon_->SetEnvironmentTexturePath(environmentTexturePath);

    ModelManager::GetInstance()->Initialize(dxCommon_, srvManager_);
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
    ModelManager::GetInstance()->CreateHeart(
        "reward_heart",
        48,
        1.0f,
        "resources/human/white.png");
    ModelManager::GetInstance()->CreateCircle(
        "effect_glow_core",
        48,
        1.0f,
        "resources/effects/glow_core.png");
    ModelManager::GetInstance()->CreateCircle(
        "effect_glow_ring",
        64,
        1.0f,
        "resources/effects/glow_ring.png");
    ModelManager::GetInstance()->CreateCircle(
        "effect_spark_star",
        48,
        1.0f,
        "resources/effects/spark_star.png");
    ModelManager::GetInstance()->CreateCircle(
        "effect_bullet_glow",
        48,
        1.0f,
        "resources/effects/glow_core.png");
    ModelManager::GetInstance()->CreatePlane(
        "effect_bullet_trail",
        1.0f,
        1.0f,
        "resources/effects/bullet_trail.png");
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
    effectGlowCoreModel_ = ModelManager::GetInstance()->FindModel("effect_glow_core");
    effectGlowRingModel_ = ModelManager::GetInstance()->FindModel("effect_glow_ring");
    effectSparkStarModel_ = ModelManager::GetInstance()->FindModel("effect_spark_star");
    effectBulletGlowModel_ = ModelManager::GetInstance()->FindModel("effect_bullet_glow");
    effectBulletTrailModel_ = ModelManager::GetInstance()->FindModel("effect_bullet_trail");

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
    hitEffects_.clear();
    player_.reset();
    camera_.reset();
    skybox_.reset();
    object3dCommon_.reset();
}

void GameRuntime::Update()
{
    if (HandleRuntimeShortcuts()) {
        return;
    }

    UpdateRailProgress();
    UpdatePlayerAndCamera();

    if (!isGameOver_ && !isGameClear_) {
        UpdatePlayerShooting();
        UpdateEnemyActions();
    }

    UpdateWorldEntities();
    UpdateGameplayCollisions();
    AdvanceEnemyWaveIfCleared();
    UpdateLockOnTarget();
    DrawHud();
    DrawResultOverlay();
    DrawEditorOverlayGuiRich();

    UpdateResultAndSceneObjects();
}

bool GameRuntime::HandleRuntimeShortcuts()
{
    if (input_ && input_->TriggerKey(DIK_F1)) {
        isEditorOverlayVisible_ = !isEditorOverlayVisible_;
        return true;
    }
    if (input_ && input_->TriggerKey(DIK_F2)) {
        isExitRequested_ = true;
        return true;
    }
    return false;
}

void GameRuntime::UpdateRailProgress()
{
    if (!isGameOver_ && !isGameClear_) {
        railDistance_ += railSpeed_;
    }
}

void GameRuntime::UpdatePlayerAndCamera()
{
    player_->Update(input_);
    player_->SetRailZ(railDistance_);
    UpdateGameCamera();
    if (skybox_) {
        skybox_->Update(camera_.get());
    }
    UpdateLockOnTarget();
}

void GameRuntime::UpdatePlayerShooting()
{
    if (shootCooldown_ > 0) {
        --shootCooldown_;
    }

    const bool isShootPressed = input_ && input_->PushKey(DIK_SPACE);
    if (isShootPressed && shootCooldown_ <= 0) {
        const bool isCharged = chargeTimer_ >= chargeShotThreshold_;
        const bool hasAssistShot = isCharged && hasLockTarget_;
        FirePlayerBullet();
        AddCameraShake(
            isCharged ? (hasAssistShot ? 0.105f : 0.085f) : 0.026f,
            isCharged ? (hasAssistShot ? 16 : 14) : 5);
        shootCooldown_ =
            isCharged ? chargedShootCooldown_ : normalShootCooldown_;
    } else if (!isShootPressed) {
        chargeTimer_ = (std::min)(chargeTimer_ + 1, kChargeShotMax);
    }

    if (chargeFlashTimer_ > 0) {
        --chargeFlashTimer_;
    }
}

void GameRuntime::UpdateEnemyActions()
{
    UpdateEnemyWave();

    --enemyShotTimer_;
    if (enemyShotTimer_ <= 0) {
        for (const auto& enemy : enemies_) {
            if (enemy->CanShoot()) {
                FireEnemyBullet(enemy->GetTranslate());
            }
        }
        enemyShotTimer_ = enemyShotInterval_;
    }
}

void GameRuntime::UpdateWorldEntities()
{
    UpdatePlayerBullets();
    UpdateEnemyBullets();
    UpdateEnemies();
    UpdateHitEffects();
    UpdateEnvironmentBonusCoins();
}

void GameRuntime::UpdateGameplayCollisions()
{
    CheckBulletBonusCoinCollisions();
    CheckPlayerBonusCoinCollisions();
    CheckBulletEnemyCollisions();
    CheckEnemyBulletPlayerCollisions();
}

void GameRuntime::UpdateResultAndSceneObjects()
{
    if (resultTransitionTimer_ > 0) {
        --resultTransitionTimer_;
    }

    for (const auto& sceneObject : sceneObjects_) {
        sceneObject->Update();
    }
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

void GameRuntime::SetHudViewportRect(
    bool isEnabled,
    const Math::Vector2& min,
    const Math::Vector2& size)
{
    isHudViewportRectEnabled_ = isEnabled && size.x > 1.0f && size.y > 1.0f;
    hudViewportMin_ = min;
    hudViewportSize_ = size;
}

void GameRuntime::GetEffectiveHudViewportRect(
    Math::Vector2& min,
    Math::Vector2& size) const
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    min = { viewport->Pos.x, viewport->Pos.y };
    size = { viewport->Size.x, viewport->Size.y };

    if (isHudViewportRectEnabled_) {
        min = hudViewportMin_;
        size = hudViewportSize_;
        return;
    }

    if (isEditorOverlayVisible_ && hasEditorOverlayViewportRect_) {
        min = editorOverlayViewportMin_;
        size = editorOverlayViewportSize_;
        return;
    }
}

void GameRuntime::InitializeEnvironmentBonusCoins()
{
    environmentBonusCoins_.clear();

    Model* coinModel =
        ModelManager::GetInstance()->FindModel("reward_heart");
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
        coin.object->SetScale({ 0.48f, 0.48f, 1.0f });
        coin.object->SetRotate({ 0.0f, 0.0f, 0.0f });
        coin.object->SetTextureFilePath("resources/human/white.png");
        coin.object->SetColor({ 1.0f, 0.16f, 0.42f, 1.0f });
        coin.object->SetLightingMode(0);
        coin.object->SetEnvironmentCoefficient(0.0f);
        coin.object->Update();
        coin.basePosition = spawn.position;
        coin.collisionRadius = 0.55f;
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

        const float pulse = 0.44f + std::sin(coin.floatPhase * 1.8f) * 0.035f;
        Math::Vector3 rotate{};
        rotate.y = std::sin(coin.floatPhase * 0.75f) * 0.18f;
        rotate.z = std::sin(coin.floatPhase * 0.55f) * 0.08f;

        coin.object->SetTranslate(position);
        coin.object->SetRotate(rotate);
        coin.object->SetScale({ pulse, pulse, 1.0f });
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

std::vector<SceneSerializer::ObjectRecord> GameRuntime::BuildRuntimeSceneRecords() const
{
    std::vector<SceneSerializer::ObjectRecord> records = sceneObjectRecords_;
    if (records.size() < sceneObjects_.size()) {
        records.resize(sceneObjects_.size());
    }

    for (size_t index = 0; index < sceneObjects_.size(); ++index) {
        const Object3d* object = sceneObjects_[index].get();
        if (!object) {
            continue;
        }

        SceneSerializer::ObjectRecord& record = records[index];
        if (record.name.empty()) {
            record.name = "Runtime Object " + std::to_string(index);
        }
        if (record.modelIndex < 0) {
            record.primitive = true;
            record.modelIndex = 0;
        }
        record.translate = object->GetTranslate();
        record.rotate = object->GetRotate();
        record.scale = object->GetScale();
        record.color = object->GetColor();
        record.alphaReference = object->GetAlphaReference();
        record.lightingMode = object->GetLightingMode();
        record.textureFilePath = object->GetTextureFilePath();
    }

    records.resize(sceneObjects_.size());
    return records;
}

SceneSerializer::SceneSettings GameRuntime::BuildRuntimeSceneSettings() const
{
    SceneSerializer::SceneSettings settings{};
    if (camera_) {
        settings.hasCamera = true;
        settings.cameraTranslate = camera_->GetTranslate();
        settings.cameraRotate = camera_->GetRotate();
    }
    return settings;
}

bool GameRuntime::SaveSceneObjects(const char* path)
{
    if (!path || path[0] == '\0') {
        editorStatusMessage_ = "保存失敗: パスが無効です。";
        return false;
    }

    const std::vector<SceneSerializer::ObjectRecord> records =
        BuildRuntimeSceneRecords();
    const bool saved =
        SceneSerializer::SaveScene(path, records, BuildRuntimeSceneSettings());
    if (!saved) {
        editorStatusMessage_ = "保存失敗: " + std::string(path);
        return false;
    }

    sceneObjectRecords_ = records;
    currentSceneFilePath_ = path;
    editorStatusMessage_ = "保存しました: " + currentSceneFilePath_;
    return true;
}

bool GameRuntime::LoadSceneObjects(const char* path)
{
    std::vector<SceneSerializer::ObjectRecord> records;
    SceneSerializer::SceneSettings settings{};
    if (!SceneSerializer::LoadScene(path, records, settings)) {
        editorStatusMessage_ = "読み込み失敗: " + std::string(path ? path : "");
        return false;
    }

    sceneObjects_.clear();
    sceneObjectRecords_.clear();

    if (settings.hasCamera && camera_) {
        camera_->SetTranslate(settings.cameraTranslate);
        camera_->SetRotate(settings.cameraRotate);
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
        sceneObjectRecords_.push_back(record);
    }

    currentSceneFilePath_ = path ? path : "";
    editorStatusMessage_ = "読み込みました: " + currentSceneFilePath_;
    return true;
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
    DrawBulletEffectObjects();
    DrawHitEffectObjects();
}

void GameRuntime::FirePlayerBullet()
{
    if (!player_ || !bulletModel_) {
        return;
    }

    Math::Vector3 spawnPosition = player_->GetTranslate();
    spawnPosition.z += 1.2f;
    const Math::Vector3 aimDirection = CalculateAimDirection(spawnPosition);
    Math::Vector3 velocity = aimDirection * playerBulletSpeed_;
    Math::Vector4 color{ 1.0f, 0.90f, 0.42f, 1.0f };
    Math::Vector3 scale{ 0.32f, 0.32f, 1.12f };
    float collisionRadius = 0.44f;
    int lifeTimer = 180;
    int hitLimit = 1;
    const bool isCharged = chargeTimer_ >= chargeShotThreshold_;

    if (isCharged) {
        velocity = aimDirection * lockBulletSpeed_ * chargedBulletSpeedMultiplier_;
        color = { 0.58f, 1.0f, 0.92f, 1.0f };
        scale = { 0.62f, 0.62f, 1.82f };
        collisionRadius = 0.82f;
        lifeTimer = 260;
        hitLimit = 1;
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
        hitLimit,
        effectBulletGlowModel_,
        isCharged ?
            Math::Vector4{ 0.18f, 0.88f, 1.0f, 0.42f } :
            Math::Vector4{ 1.0f, 0.82f, 0.30f, 0.34f },
        isCharged ?
            Math::Vector3{ 1.08f, 1.08f, 1.0f } :
            Math::Vector3{ 0.58f, 0.58f, 1.0f },
        effectBulletGlowModel_,
        isCharged ?
            Math::Vector4{ 0.20f, 0.94f, 1.0f, 0.62f } :
            Math::Vector4{ 1.0f, 0.72f, 0.16f, 0.46f },
        isCharged ?
            Math::Vector3{ 0.38f, 1.95f, 1.0f } :
            Math::Vector3{ 0.22f, 1.18f, 1.0f },
        isCharged ? 1.42f : 0.82f,
        effectSparkStarModel_,
        isCharged ?
            Math::Vector4{ 0.82f, 1.0f, 0.92f, 0.76f } :
            Math::Vector4{ 1.0f, 0.92f, 0.46f, 0.52f },
        isCharged ?
            Math::Vector3{ 0.20f, 0.20f, 1.0f } :
            Math::Vector3{ 0.12f, 0.12f, 1.0f });
    if (isCharged) {
        bullet->EnableHoming(0.075f);
        if (lockedEnemy_) {
            bullet->SetHomingTarget(lockedEnemy_->GetTranslate());
        }
    }
    playerBullets_.push_back(std::move(bullet));
    AddMuzzleFlashEffect(spawnPosition, isCharged);
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
        { 0.0f, 0.0f, -enemyBulletSpeed_ },
        { 1.0f, 0.18f, 0.14f, 1.0f },
        240,
        { 0.56f, 0.56f, 0.54f },
        0.55f,
        1,
        effectBulletGlowModel_,
        { 1.0f, 0.10f, 0.08f, 0.22f },
        { 0.58f, 0.58f, 1.0f },
        effectBulletGlowModel_,
        { 1.0f, 0.08f, 0.06f, 0.24f },
        { 0.15f, 0.82f, 1.0f },
        0.56f);
    enemyBullets_.push_back(std::move(bullet));
}

void GameRuntime::SpawnEnemy()
{
    if (!enemyModel_ || currentWaveIndex_ >= kWaveCount) {
        return;
    }

    const WaveTuning& wave = waveTuning_[currentWaveIndex_];
    const EnemySpawnPattern pattern =
        GetEnemySpawnPattern(currentWaveIndex_, spawnSequenceIndex_);
    ++spawnSequenceIndex_;
    ++spawnedEnemyCountInWave_;

    auto enemy = std::make_unique<Enemy>();
    enemy->Initialize(
        object3dCommon_.get(),
        enemyModel_,
        { pattern.x, pattern.y, railDistance_ + wave.spawnLeadDistance },
        pattern.behavior,
        pattern.entryStyle);
    enemies_.push_back(std::move(enemy));
}

void GameRuntime::UpdateEnemyWave()
{
    if (currentWaveIndex_ >= kWaveCount) {
        return;
    }

    const WaveTuning& wave = waveTuning_[currentWaveIndex_];
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

    const WaveTuning& wave = waveTuning_[currentWaveIndex_];
    if (defeatedEnemyCountInWave_ < wave.enemyCount || !enemies_.empty()) {
        return;
    }

    ++currentWaveIndex_;
    spawnedEnemyCountInWave_ = 0;
    defeatedEnemyCountInWave_ = 0;
    enemySpawnTimer_ = waveStartDelay_;

    if (currentWaveIndex_ >= kWaveCount) {
        isGameClear_ = true;
        resultTransitionTimer_ = 90;
    }
}

void GameRuntime::UpdateLockOnTarget()
{
    lockedEnemy_ = nullptr;
    hasLockTarget_ = false;
    isReticleOnTarget_ = false;

    Math::Vector2 viewportMin{};
    Math::Vector2 viewportSize{};
    GetEffectiveHudViewportRect(viewportMin, viewportSize);
    const Math::Vector2 viewportMax{
        viewportMin.x + viewportSize.x,
        viewportMin.y + viewportSize.y
    };
    if (input_) {
        Math::Vector2 mouseScreen{};
        if (isEditorOverlayVisible_ && hasEditorOverlayViewportRect_) {
            const ImVec2 imguiMouse = ImGui::GetMousePos();
            mouseScreen = { imguiMouse.x, imguiMouse.y };
        } else {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            const Math::Vector2& mousePosition = input_->GetMousePosition();
            mouseScreen = {
                viewport->Pos.x + mousePosition.x,
                viewport->Pos.y + mousePosition.y
            };
        }
        reticleScreen_ = {
            std::clamp(mouseScreen.x, viewportMin.x, viewportMax.x),
            std::clamp(mouseScreen.y, viewportMin.y, viewportMax.y)
        };
    } else {
        reticleScreen_ = {
            viewportMin.x + viewportSize.x * 0.5f,
            viewportMin.y + viewportSize.y * 0.5f
        };
    }

    if (!camera_ || isGameOver_ || isGameClear_) {
        return;
    }

    const float lockRadius = (std::max)(lockRadius_, 1.0f);
    const float kLockRadiusSq = lockRadius * lockRadius;
    constexpr float kReticleHitRadius = 24.0f;
    constexpr float kReticleHitRadiusSq = kReticleHitRadius * kReticleHitRadius;
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
            isReticleOnTarget_ = distanceSq <= kReticleHitRadiusSq;
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

    Math::Vector2 viewportMin{};
    Math::Vector2 viewportSize{};
    GetEffectiveHudViewportRect(viewportMin, viewportSize);
    screenPosition = {
        viewportMin.x + (ndcX + 1.0f) * 0.5f * viewportSize.x,
        viewportMin.y + (1.0f - ndcY) * 0.5f * viewportSize.y
    };
    return true;
}

void GameRuntime::AddEnemyHitEffect(
    const Math::Vector3& worldPosition,
    float strength)
{
    HitEffect effect{};
    effect.worldPosition = worldPosition;
    effect.duration = 34;
    effect.strength = strength;
    effect.scoreValue = 100;
    effect.type = HitEffectType::EnemyDestroy;

    AddHitEffectVisual(effect, effectGlowCoreModel_, worldPosition,
        { 1.0f, 1.0f, 0.92f, 0.88f }, 0.48f, 0.78f, 0.0f, 0.0f, 1.0f, 1.0f, {});
    AddHitEffectVisual(effect, effectGlowCoreModel_, worldPosition,
        { 0.42f, 0.88f, 1.0f, 0.34f }, 0.74f, 0.36f, 0.0f, 0.05f, 1.0f, 1.0f, {});
    AddHitEffectVisual(effect, effectSparkStarModel_, worldPosition,
        { 1.0f, 0.96f, 0.72f, 0.82f }, 0.22f, 0.46f, -1.40f, 0.00f, 1.00f, 0.74f, { -0.070f, 0.055f, 0.010f });
    AddHitEffectVisual(effect, effectSparkStarModel_, worldPosition,
        { 0.72f, 0.96f, 1.0f, 0.74f }, 0.20f, 0.42f, 1.65f, 0.02f, 0.92f, 0.70f, { 0.078f, 0.046f, 0.010f });
    AddHitEffectVisual(effect, effectSparkStarModel_, worldPosition,
        { 1.0f, 0.78f, 0.34f, 0.68f }, 0.18f, 0.38f, -2.10f, 0.03f, 0.88f, 0.66f, { -0.050f, -0.062f, 0.006f });
    AddHitEffectVisual(effect, effectSparkStarModel_, worldPosition,
        { 0.56f, 0.86f, 1.0f, 0.64f }, 0.17f, 0.36f, 2.25f, 0.05f, 0.84f, 0.64f, { 0.052f, -0.058f, 0.006f });
    AddHitEffectVisual(effect, effectSparkStarModel_, worldPosition,
        { 1.0f, 1.0f, 0.88f, 0.58f }, 0.16f, 0.34f, 0.95f, 0.08f, 0.78f, 0.60f, { 0.000f, 0.082f, 0.004f });
    AddHitEffectVisual(effect, effectSparkStarModel_, worldPosition,
        { 0.48f, 0.80f, 1.0f, 0.52f }, 0.15f, 0.30f, -0.80f, 0.10f, 0.76f, 0.58f, { 0.000f, -0.078f, 0.004f });
    hitEffects_.push_back(std::move(effect));
}

void GameRuntime::AddEnemyImpactEffect(
    const Math::Vector3& worldPosition,
    float strength)
{
    HitEffect effect{};
    effect.worldPosition = worldPosition;
    effect.duration = 20;
    effect.strength = strength;
    effect.type = HitEffectType::EnemyImpact;

    AddHitEffectVisual(effect, effectGlowCoreModel_, worldPosition,
        { 1.0f, 0.96f, 0.72f, 0.66f }, 0.34f, 0.30f, 0.0f, 0.0f, 1.0f, 1.0f, {});
    AddHitEffectVisual(effect, effectSparkStarModel_, worldPosition,
        { 0.72f, 0.96f, 1.0f, 0.72f }, 0.28f, 0.56f, -1.05f, 0.0f, 1.18f, 0.62f, { -0.020f, 0.018f, 0.0f });
    AddHitEffectVisual(effect, effectSparkStarModel_, worldPosition,
        { 1.0f, 0.78f, 0.22f, 0.58f }, 0.22f, 0.48f, 1.20f, 0.04f, 0.94f, 0.58f, { 0.024f, -0.014f, 0.0f });

    hitEffects_.push_back(std::move(effect));
}

void GameRuntime::AddMuzzleFlashEffect(
    const Math::Vector3& worldPosition,
    bool isCharged)
{
    HitEffect effect{};
    effect.worldPosition = worldPosition;
    effect.duration = isCharged ? 18 : 12;
    effect.strength = isCharged ? 1.15f : 0.78f;
    effect.type = HitEffectType::EnemyImpact;

    AddHitEffectVisual(effect, effectGlowCoreModel_, worldPosition,
        isCharged ?
            Math::Vector4{ 0.48f, 0.96f, 1.0f, 0.54f } :
            Math::Vector4{ 1.0f, 0.82f, 0.30f, 0.42f },
        isCharged ? 0.38f : 0.24f,
        isCharged ? 0.28f : 0.18f,
        0.0f,
        0.0f,
        1.25f,
        0.72f,
        {});
    AddHitEffectVisual(effect, effectSparkStarModel_, worldPosition,
        isCharged ?
            Math::Vector4{ 0.72f, 1.0f, 0.92f, 0.58f } :
            Math::Vector4{ 1.0f, 0.92f, 0.46f, 0.48f },
        isCharged ? 0.24f : 0.16f,
        0.36f,
        isCharged ? 1.4f : -1.0f,
        0.02f,
        1.0f,
        0.58f,
        {});

    hitEffects_.push_back(std::move(effect));
}

void GameRuntime::AddCoinCollectEffect(const Math::Vector3& worldPosition)
{
    HitEffect effect{};
    effect.worldPosition = worldPosition;
    effect.duration = 36;
    effect.strength = 1.0f;
    effect.scoreValue = 250;
    effect.type = HitEffectType::CoinCollect;

    AddHitEffectVisual(effect, effectGlowCoreModel_, worldPosition,
        { 1.0f, 0.74f, 0.18f, 1.0f }, 0.62f, 0.25f, 0.0f, 0.0f, 1.0f, 1.0f, { 0.0f, 0.035f, 0.0f });
    AddHitEffectVisual(effect, effectSparkStarModel_, worldPosition,
        { 1.0f, 0.90f, 0.40f, 0.82f }, 0.72f, 0.92f, 2.35f, 0.05f, 1.35f, 0.78f, { 0.0f, 0.09f, 0.02f });
    AddHitEffectVisual(effect, effectGlowRingModel_, worldPosition,
        { 1.0f, 0.54f, 0.08f, 0.54f }, 0.52f, 1.85f, -1.55f, 0.16f, 1.0f, 1.0f, { 0.0f, 0.02f, 0.0f });
    hitEffects_.push_back(std::move(effect));
}

void GameRuntime::AddPlayerDamageEffect(const Math::Vector3& worldPosition)
{
    HitEffect effect{};
    effect.worldPosition = worldPosition;
    effect.duration = 28;
    effect.strength = 1.0f;
    effect.type = HitEffectType::PlayerDamage;

    AddHitEffectVisual(effect, effectGlowCoreModel_, worldPosition,
        { 1.0f, 0.16f, 0.12f, 0.72f }, 1.15f, 0.36f, 0.0f, 0.0f, 1.25f, 0.72f, { 0.0f, 0.02f, -0.02f });
    AddHitEffectVisual(effect, effectSparkStarModel_, worldPosition,
        { 1.0f, 0.32f, 0.24f, 0.48f }, 0.95f, 0.82f, -1.35f, 0.10f, 1.45f, 0.65f, { 0.0f, 0.03f, 0.0f });
    hitEffects_.push_back(std::move(effect));
}

void GameRuntime::AddHitEffectVisual(
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
    const Math::Vector3& velocity)
{
    if (!object3dCommon_ || !model) {
        return;
    }

    HitEffect::Visual visual{};
    visual.object = std::make_unique<Object3d>();
    visual.object->Initialize(object3dCommon_.get());
    visual.object->SetModel(model);
    visual.object->SetTranslate(worldPosition);
    visual.object->SetScale({
        baseSize * aspectX * effect.strength,
        baseSize * aspectY * effect.strength,
        1.0f
    });
    visual.object->SetRotate(camera_ ? camera_->GetRotate() : Math::Vector3{});
    visual.object->SetColor(color);
    visual.object->SetLightingMode(0);
    visual.object->SetEnvironmentCoefficient(0.0f);
    visual.object->Update();
    visual.color = color;
    visual.baseSize = baseSize;
    visual.growth = growth;
    visual.spin = spin;
    visual.popDelay = popDelay;
    visual.aspectX = aspectX;
    visual.aspectY = aspectY;
    visual.velocity = velocity;
    effect.visuals.push_back(std::move(visual));
}

void GameRuntime::UpdateHitEffects()
{
    for (HitEffect& effect : hitEffects_) {
        ++effect.age;
    }

    hitEffects_.erase(
        std::remove_if(
            hitEffects_.begin(),
            hitEffects_.end(),
            [](const HitEffect& effect) {
                return effect.age >= effect.duration;
            }),
        hitEffects_.end());
}

void GameRuntime::DrawHitEffects()
{
    return;
}

void GameRuntime::DrawBulletEffectObjects()
{
    if (!object3dCommon_ || !camera_) {
        return;
    }

    const BlendMode previousBlendMode = object3dCommon_->GetBlendMode();
    const DepthDrawMode previousDepthMode = object3dCommon_->GetDepthDrawMode();

    object3dCommon_->SetDepthDrawMode(DepthDrawMode::Normal);
    object3dCommon_->SetBlendMode(BlendMode::Add);
    object3dCommon_->CommonDrawSetting();

    const Math::Vector3 cameraRotate = camera_->GetRotate();
    for (const auto& bullet : playerBullets_) {
        if (bullet) {
            bullet->DrawGlow(cameraRotate);
        }
    }
    for (const auto& bullet : enemyBullets_) {
        if (bullet) {
            bullet->DrawGlow(cameraRotate);
        }
    }

    object3dCommon_->SetBlendMode(previousBlendMode);
    object3dCommon_->SetDepthDrawMode(previousDepthMode);
    object3dCommon_->CommonDrawSetting();
}

void GameRuntime::DrawHitEffectObjects()
{
    if (hitEffects_.empty() || !object3dCommon_) {
        return;
    }

    const BlendMode previousBlendMode = object3dCommon_->GetBlendMode();
    const DepthDrawMode previousDepthMode = object3dCommon_->GetDepthDrawMode();

    object3dCommon_->SetDepthDrawMode(DepthDrawMode::Overlay);
    object3dCommon_->SetBlendMode(BlendMode::Add);
    object3dCommon_->CommonDrawSetting();

    for (const HitEffect& effect : hitEffects_) {
        if (effect.visuals.empty()) {
            continue;
        }

        const float rate =
            static_cast<float>(effect.age) /
            static_cast<float>((std::max)(effect.duration, 1));
        for (const HitEffect::Visual& visual : effect.visuals) {
            if (!visual.object) {
                continue;
            }
            if (rate < visual.popDelay) {
                continue;
            }
            const float localRate = (std::clamp)(
                (rate - visual.popDelay) / (std::max)(1.0f - visual.popDelay, 0.001f),
                0.0f,
                1.0f);
            const float fade = 1.0f - localRate;
            const float easeOut = 1.0f - std::pow(1.0f - localRate, 3.0f);
            const float size =
                (visual.baseSize + visual.baseSize * visual.growth * easeOut) *
                effect.strength;
            Math::Vector4 color = visual.color;
            const float alphaCurve =
                effect.type == HitEffectType::CoinCollect ? fade :
                effect.type == HitEffectType::PlayerDamage ? fade * fade * fade :
                fade * fade;
            color.w *= alphaCurve;

            Math::Vector3 rotate = camera_ ? camera_->GetRotate() : Math::Vector3{};
            rotate.z += localRate * visual.spin;
            Math::Vector3 translate = effect.worldPosition;
            translate.x += visual.velocity.x * static_cast<float>(effect.age);
            translate.y += visual.velocity.y * static_cast<float>(effect.age);
            translate.z += visual.velocity.z * static_cast<float>(effect.age);

            visual.object->SetTranslate(translate);
            visual.object->SetScale({
                size * visual.aspectX,
                size * visual.aspectY,
                1.0f
            });
            visual.object->SetRotate(rotate);
            visual.object->SetColor(color);
            visual.object->Update();
            visual.object->Draw();
        }
    }

    object3dCommon_->SetBlendMode(previousBlendMode);
    object3dCommon_->SetDepthDrawMode(previousDepthMode);
    object3dCommon_->CommonDrawSetting();
}

void GameRuntime::DrawEditorOverlayGuiRich()
{
    if (!isEditorOverlayVisible_) {
        hasEditorOverlayViewportRect_ = false;
        return;
    }

    hasEditorOverlayViewportRect_ = false;

    const char* kWindowHierarchy = "ヒエラルキー";
    const char* kWindowInspector = "インスペクター";
    const char* kWindowGameView = "ゲームビュー";
    const char* kWindowProject = "プロジェクト";
    const char* kWindowConsole = "コンソール";
    const char* kWindowStats = "統計";
    const char* kWindowTuning = "ゲーム調整";
    const char* kWindowNodeGraph = "ノードグラフ";
    const char* kWindowTextView = "テキスト表示";

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImGuiID dockspaceId = ImGui::GetID("GameOverlayDockSpace");
    const ImGuiDockNodeFlags dockspaceFlags =
        ImGuiDockNodeFlags_NoWindowMenuButton |
        ImGuiDockNodeFlags_NoCloseButton;
    bool requestDockLayoutReset = false;
    static bool showAdvancedDebugPanels = false;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("\xE3\x83\x95\xE3\x82\xA1\xE3\x82\xA4\xE3\x83\xAB")) {
            if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " \xE8\xAA\xAD\xE3\x81\xBF\xE8\xBE\xBC\xE3\x81\xBF", "Ctrl+O")) {
                IGFD::FileDialogConfig config{};
                config.path = "resources";
                ImGuiFileDialog::Instance()->OpenDialog(
                    "GameSceneOpenDialog",
                    "シーンを開く",
                    ".json",
                    config);
            }
            if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " \xE4\xBF\x9D\xE5\xAD\x98", "Ctrl+S")) {
                SaveSceneObjects(currentSceneFilePath_.c_str());
            }
            if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " 名前を付けて保存")) {
                IGFD::FileDialogConfig config{};
                config.path = "resources";
                config.fileName = "game_scene.json";
                ImGuiFileDialog::Instance()->OpenDialog(
                    "GameSceneSaveDialog",
                    "名前を付けてシーンを保存",
                    ".json",
                    config);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("\xE3\x82\xBF\xE3\x82\xA4\xE3\x83\x88\xE3\x83\xAB\xE3\x81\xB8\xE6\x88\xBB\xE3\x82\x8B", "F2")) {
                isExitRequested_ = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("\xE7\xB7\xA8\xE9\x9B\x86")) {
            ImGui::MenuItem(ICON_FA_ROTATE_LEFT " \xE5\x85\x83\xE3\x81\xAB\xE6\x88\xBB\xE3\x81\x99", "Ctrl+Z", false, false);
            ImGui::MenuItem(ICON_FA_ROTATE_RIGHT " \xE3\x82\x84\xE3\x82\x8A\xE7\x9B\xB4\xE3\x81\x97", "Ctrl+Y", false, false);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("\xE3\x82\xB2\xE3\x83\xBC\xE3\x83\xA0\xE3\x82\xAA\xE3\x83\x96\xE3\x82\xB8\xE3\x82\xA7\xE3\x82\xAF\xE3\x83\x88")) {
            ImGui::MenuItem(ICON_FA_USER " プレイヤー", nullptr, false, false);
            ImGui::MenuItem(ICON_FA_BULLSEYE " 敵", nullptr, false, false);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("\xE3\x82\xA6\xE3\x82\xA3\xE3\x83\xB3\xE3\x83\x89\xE3\x82\xA6")) {
            if (ImGui::MenuItem(ICON_FA_TABLE_COLUMNS " \xE5\x88\x9D\xE6\x9C\x9F\xE3\x83\xAC\xE3\x82\xA4\xE3\x82\xA2\xE3\x82\xA6\xE3\x83\x88\xE3\x81\xAB\xE6\x88\xBB\xE3\x81\x99")) {
                requestDockLayoutReset = true;
            }
            ImGui::MenuItem(
                ICON_FA_CODE_BRANCH " 詳細デバッグパネル",
                nullptr,
                &showAdvancedDebugPanels);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("\xE5\x86\x8D\xE7\x94\x9F")) {
            ImGui::MenuItem(ICON_FA_PLAY " \xE3\x82\xB2\xE3\x83\xBC\xE3\x83\xA0\xE8\xA1\xA8\xE7\xA4\xBA\xE4\xB8\xAD", nullptr, true, false);
            ImGui::EndMenu();
        }
        ImGui::Separator();
        ImGui::TextDisabled("F1 GUI表示切り替え / F2 タイトルへ戻る");
        ImGui::EndMainMenuBar();
    }

    ImGui::DockSpaceOverViewport(dockspaceId, viewport, dockspaceFlags);

    static bool hasBuiltDefaultDockLayout = false;
    if (!hasBuiltDefaultDockLayout || requestDockLayoutReset) {
        hasBuiltDefaultDockLayout = true;
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, dockspaceFlags | ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);

        ImGuiID mainId = dockspaceId;
        ImGuiID leftId = 0;
        ImGuiID rightId = 0;
        ImGuiID rightBottomId = 0;
        ImGuiID bottomId = 0;
        ImGuiID centerId = 0;
        leftId = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Left, 0.22f, nullptr, &mainId);
        rightId = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Right, 0.27f, nullptr, &mainId);
        bottomId = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Down, 0.24f, nullptr, &centerId);
        rightBottomId = ImGui::DockBuilderSplitNode(rightId, ImGuiDir_Down, 0.48f, nullptr, &rightId);

        ImGui::DockBuilderDockWindow(kWindowHierarchy, leftId);
        ImGui::DockBuilderDockWindow(kWindowInspector, rightId);
        ImGui::DockBuilderDockWindow(kWindowProject, bottomId);
        ImGui::DockBuilderDockWindow(kWindowConsole, bottomId);
        ImGui::DockBuilderDockWindow(kWindowTuning, rightBottomId);
        ImGui::DockBuilderDockWindow(kWindowStats, rightBottomId);
        ImGui::DockBuilderDockWindow(kWindowNodeGraph, bottomId);
        ImGui::DockBuilderDockWindow(kWindowTextView, bottomId);
        ImGui::DockBuilderDockWindow(kWindowGameView, centerId);
        ImGui::DockBuilderFinish(dockspaceId);
    }

    const ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin(kWindowHierarchy, nullptr, panelFlags)) {
        ImGui::TextUnformatted("検索");
        ImGui::SameLine();
        ImGui::Button(ICON_FA_PLUS, ImVec2(32.0f, 0.0f));
        ImGui::SameLine();
        ImGui::Button(ICON_FA_MINUS, ImVec2(32.0f, 0.0f));
        ImGui::Separator();
        ImGui::Selectable(ICON_FA_USER " プレイヤー", true);
        ImGui::Selectable((std::string(ICON_FA_BULLSEYE " 敵 x ") + std::to_string(enemies_.size())).c_str(), false);
        ImGui::Selectable((std::string(ICON_FA_CIRCLE " 自弾 x ") + std::to_string(playerBullets_.size())).c_str(), false);
        ImGui::Selectable((std::string(ICON_FA_CIRCLE_DOT " 敵弾 x ") + std::to_string(enemyBullets_.size())).c_str(), false);
        ImGui::Selectable((std::string(ICON_FA_HEART " 報酬ハート x ") + std::to_string(environmentBonusCoins_.size())).c_str(), false);
        ImGui::Selectable((std::string(ICON_FA_CUBE " シーンオブジェクト x ") + std::to_string(sceneObjects_.size())).c_str(), false);
        ImGui::Separator();
        ImGui::Text("ウェーブ: %d / %d", (std::min)(currentWaveIndex_ + 1, kWaveCount), kWaveCount);
        ImGui::Text(
            "出現数: %d / %d",
            currentWaveIndex_ < kWaveCount ? spawnedEnemyCountInWave_ : 0,
            currentWaveIndex_ < kWaveCount ? waveTuning_[currentWaveIndex_].enemyCount : 0);
    }
    ImGui::End();

    if (ImGui::Begin(kWindowInspector, nullptr, panelFlags)) {
        if (ImGui::CollapsingHeader(ICON_FA_PALETTE " 描画", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox(ICON_FA_EYE " スカイボックス", &showSkybox_);
            const char* postEffectItems[] = {
                "なし",
                "グレースケール",
                "ビネット",
                "ボックス 3x3",
                "ボックス 5x5",
                "ガウシアン",
                "輝度アウトライン",
                "深度アウトライン",
                "ラジアルブラー",
                "ディゾルブ",
                "ランダム",
                "ビネット + スムージング",
                "ゲームトーン 自動",
                "ゲームトーン 低HP",
                "ゲームトーン クリア",
                "ゲームトーン ゲームオーバー",
                "ブルーム"
            };
            ImGui::Combo("ポストエフェクト", &postEffectMode_, postEffectItems, IM_ARRAYSIZE(postEffectItems));
        }
        if (ImGui::CollapsingHeader(ICON_FA_USER " プレイヤー", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("HP: %d / %d", GetPlayerHp(), GetPlayerMaxHp());
            ImGui::Text("スコア: %d", score_);
            ImGui::Text("チャージ: %s", chargeTimer_ >= chargeShotThreshold_ ? "完了" : "蓄積中");
            ImGui::Text("チャージ補助: %s", hasLockTarget_ ? "ON" : "OFF");
        }
        if (ImGui::CollapsingHeader(ICON_FA_LOCATION_DOT " トランスフォーム", ImGuiTreeNodeFlags_DefaultOpen)) {
            const Math::Vector3 playerPosition = player_ ? player_->GetTranslate() : Math::Vector3{};
            ImGui::Text("位置 X: %.2f", playerPosition.x);
            ImGui::Text("位置 Y: %.2f", playerPosition.y);
            ImGui::Text("位置 Z: %.2f", playerPosition.z);
            ImGui::Text("レール距離: %.1f", railDistance_);
        }
        if (ImGui::CollapsingHeader(ICON_FA_GAMEPAD " ゲーム進行", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("撃破数: %d / %d", defeatedEnemyCount_, GetTotalEnemyTargetCount());
            ImGui::Text("敵数: %zu", enemies_.size());
            ImGui::Text("自弾: %zu", playerBullets_.size());
            ImGui::Text("敵弾: %zu", enemyBullets_.size());
        }
    }
    ImGui::End();

    if (ImGui::Begin(kWindowGameView, nullptr, panelFlags)) {
        const ImVec2 contentMin = ImGui::GetCursorScreenPos();
        const ImVec2 contentAvail = ImGui::GetContentRegionAvail();
        constexpr float kGameViewAspect = 16.0f / 9.0f;
        ImVec2 imageSize(
            (std::max)(contentAvail.x, 1.0f),
            (std::max)(contentAvail.y, 1.0f));
        if (imageSize.x / imageSize.y > kGameViewAspect) {
            imageSize.x = imageSize.y * kGameViewAspect;
        } else {
            imageSize.y = imageSize.x / kGameViewAspect;
        }
        const ImVec2 imageOffset(
            (std::max)(contentAvail.x - imageSize.x, 0.0f) * 0.5f,
            (std::max)(contentAvail.y - imageSize.y, 0.0f) * 0.5f);
        ImGui::SetCursorScreenPos(
            ImVec2(contentMin.x + imageOffset.x, contentMin.y + imageOffset.y));
        if (dxCommon_) {
            ImGui::Image(
                static_cast<ImTextureID>(dxCommon_->GetRenderTextureGpuDescriptorHandle().ptr),
                imageSize);
            editorOverlayViewportMin_ = {
                contentMin.x + imageOffset.x,
                contentMin.y + imageOffset.y
            };
            editorOverlayViewportSize_ = { imageSize.x, imageSize.y };
            hasEditorOverlayViewportRect_ = imageSize.x > 1.0f && imageSize.y > 1.0f;
        }
    }
    ImGui::End();

    if (ImGui::Begin(kWindowProject, nullptr, panelFlags)) {
        if (ImGui::Button(ICON_FA_FOLDER_OPEN " 開く")) {
            IGFD::FileDialogConfig config{};
            config.path = "resources";
            ImGuiFileDialog::Instance()->OpenDialog(
                "ProjectAssetDialog",
                "アセットを開く",
                ".json,.png,.gltf,.obj,.dds",
                config);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("ImGuiFileDialog");
        ImGui::Text("現在のシーン: %s", currentSceneFilePath_.c_str());
        ImGui::TextDisabled("%s", editorStatusMessage_.c_str());
        ImGui::Separator();
        ImGui::Columns(4, "GameOverlayAssetsRich", false);
        ImGui::TextUnformatted("シーン");
        ImGui::NextColumn();
        ImGui::TextUnformatted("プレハブ");
        ImGui::NextColumn();
        ImGui::TextUnformatted("モデル");
        ImGui::NextColumn();
        ImGui::TextUnformatted("テクスチャ");
        ImGui::NextColumn();
        ImGui::Separator();
        ImGui::TextUnformatted("resources/game_scene.json");
        ImGui::NextColumn();
        ImGui::TextUnformatted("resources/prefab_00.json");
        ImGui::NextColumn();
        ImGui::TextUnformatted("game_player");
        ImGui::NextColumn();
        ImGui::TextUnformatted("resources/checkerBoard.png");
        ImGui::NextColumn();
        ImGui::TextUnformatted("resources/scene_01.json");
        ImGui::NextColumn();
        ImGui::TextUnformatted("resources/prefab_01.json");
        ImGui::NextColumn();
        ImGui::TextUnformatted("primitive_sphere");
        ImGui::NextColumn();
        ImGui::TextUnformatted("resources/uvChecker.png");
        ImGui::Columns(1);
    }
    ImGui::End();

    if (ImGui::Begin(kWindowStats, nullptr, panelFlags)) {
        static float historyTime[120]{};
        static float enemyHistory[120]{};
        static float bulletHistory[120]{};
        static int historyOffset = 0;
        historyTime[historyOffset] = static_cast<float>(historyOffset);
        enemyHistory[historyOffset] = static_cast<float>(enemies_.size());
        bulletHistory[historyOffset] =
            static_cast<float>(playerBullets_.size() + enemyBullets_.size());
        historyOffset = (historyOffset + 1) % 120;

        ImGui::Text("スコア: %d", score_);
        ImGui::Text("敵数: %zu", enemies_.size());
        ImGui::Text("弾数: %zu", playerBullets_.size() + enemyBullets_.size());
        if (ImPlot::BeginPlot("実行中カウント", ImVec2(-1.0f, 170.0f))) {
            ImPlot::SetupAxes("フレーム", "数", ImPlotAxisFlags_NoTickLabels, 0);
            ImPlot::PlotLine("敵", historyTime, enemyHistory, 120);
            ImPlot::PlotLine("弾", historyTime, bulletHistory, 120);
            ImPlot::EndPlot();
        }
    }
    ImGui::End();

    if (ImGui::Begin(kWindowTuning, nullptr, panelFlags)) {
        if (ImGui::Button(ICON_FA_ROTATE_LEFT " 調整をリセット")) {
            waveTuning_ = { {
                { 6, 80, 26.0f },
                { 8, 60, 30.0f },
                { 10, 45, 34.0f }
            } };
            railSpeed_ = 0.045f;
            playerBulletSpeed_ = 0.65f;
            lockBulletSpeed_ = 0.82f;
            chargedBulletSpeedMultiplier_ = 1.18f;
            enemyBulletSpeed_ = 0.28f;
            lockRadius_ = 118.0f;
            chargeShotThreshold_ = 70;
            normalShootCooldown_ = 8;
            chargedShootCooldown_ = 14;
            enemyShotInterval_ = 75;
            waveStartDelay_ = 90;
            editorStatusMessage_ = "ゲーム調整をリセットしました。";
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_FORWARD_STEP " 今すぐ出現")) {
            enemySpawnTimer_ = 0;
        }

        if (ImGui::CollapsingHeader(ICON_FA_WAND_MAGIC_SPARKLES " 操作感", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("レール速度", &railSpeed_, 0.010f, 0.120f, "%.3f");
            ImGui::SliderFloat("自弾速度", &playerBulletSpeed_, 0.30f, 1.20f, "%.2f");
            ImGui::SliderFloat("チャージ弾速度", &lockBulletSpeed_, 0.70f, 2.20f, "%.2f");
            ImGui::SliderFloat("チャージ弾速度倍率", &chargedBulletSpeedMultiplier_, 1.00f, 2.00f, "%.2f");
            ImGui::SliderFloat("敵弾速度", &enemyBulletSpeed_, 0.12f, 0.70f, "%.2f");
            ImGui::SliderFloat("チャージ補助範囲", &lockRadius_, 40.0f, 260.0f, "%.0f px");
            ImGui::SliderInt("チャージ必要フレーム", &chargeShotThreshold_, 20, kChargeShotMax);
            ImGui::SliderInt("通常射撃クールダウン", &normalShootCooldown_, 2, 24);
            ImGui::SliderInt("チャージ射撃クールダウン", &chargedShootCooldown_, 4, 36);
            ImGui::SliderInt("敵射撃間隔", &enemyShotInterval_, 20, 180);
        }

        if (ImGui::CollapsingHeader(ICON_FA_LAYER_GROUP " ウェーブ", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderInt("ウェーブ開始待ち", &waveStartDelay_, 0, 180);
            for (int index = 0; index < kWaveCount; ++index) {
                ImGui::PushID(index);
                char label[32]{};
                std::snprintf(label, sizeof(label), "ウェーブ %d", index + 1);
                if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::SliderInt("敵数", &waveTuning_[index].enemyCount, 1, 30);
                    ImGui::SliderInt("出現間隔", &waveTuning_[index].spawnInterval, 10, 180);
                    ImGui::SliderFloat("出現距離", &waveTuning_[index].spawnLeadDistance, 12.0f, 60.0f, "%.1f");
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }

        ImGui::Separator();
        ImGui::Text("現在のウェーブ: %d / %d", (std::min)(currentWaveIndex_ + 1, kWaveCount), kWaveCount);
        ImGui::Text("出現済み: %d", spawnedEnemyCountInWave_);
        ImGui::Text("次の出現タイマー: %d", enemySpawnTimer_);
        ImGui::Text("合計敵数: %d", GetTotalEnemyTargetCount());
    }
    ImGui::End();

    if (showAdvancedDebugPanels) {
        if (ImGui::Begin(kWindowNodeGraph, &showAdvancedDebugPanels, panelFlags)) {
            namespace ed = ax::NodeEditor;
            static ed::EditorContext* editorContext = ed::CreateEditor();
            ed::SetCurrentEditor(editorContext);
            ed::Begin("RuntimeFlow");
            ed::BeginNode(1);
            ImGui::TextUnformatted(ICON_FA_PLAY " 実行処理");
            ed::BeginPin(11, ed::PinKind::Output);
            ImGui::TextUnformatted("更新");
            ed::EndPin();
            ed::EndNode();
            ed::BeginNode(2);
            ImGui::TextUnformatted(ICON_FA_CHART_LINE " 統計");
            ed::BeginPin(21, ed::PinKind::Input);
            ImGui::Text("スコア %d", score_);
            ed::EndPin();
            ed::EndNode();
            ed::Link(100, 11, 21);
            ed::End();
            ed::SetCurrentEditor(nullptr);
        }
        ImGui::End();

        if (ImGui::Begin(kWindowTextView, &showAdvancedDebugPanels, panelFlags)) {
            static TextEditor editor;
            static bool isEditorInitialized = false;
            if (!isEditorInitialized) {
                editor.SetLanguageDefinition(TextEditor::LanguageDefinition::HLSL());
                editor.SetReadOnly(true);
                editor.SetText(
                    "{\n"
                    "  \"scene\": \"resources/game_scene.json\",\n"
                    "  \"score\": 0,\n"
                    "  \"features\": [\"ImGuiFileDialog\", \"ImPlot\", \"NodeEditor\", \"ColorTextEdit\"]\n"
                    "}\n");
                isEditorInitialized = true;
            }
            static std::string lastTextViewScenePath;
            static int lastTextViewScore = -1;
            if (lastTextViewScenePath != currentSceneFilePath_ ||
                lastTextViewScore != score_) {
                char buffer[512]{};
                std::snprintf(
                    buffer,
                    sizeof(buffer),
                    "{\n"
                    "  \"scene\": \"%s\",\n"
                    "  \"score\": %d,\n"
                    "  \"sceneObjects\": %zu,\n"
                    "  \"features\": [\"ImGuiFileDialog\", \"ImPlot\", \"NodeEditor\", \"ColorTextEdit\"]\n"
                    "}\n",
                    currentSceneFilePath_.c_str(),
                    score_,
                    sceneObjects_.size());
                editor.SetText(buffer);
                lastTextViewScenePath = currentSceneFilePath_;
                lastTextViewScore = score_;
            }
            ImGui::TextDisabled("ImGuiColorTextEdit");
            editor.Render("RuntimeJsonPreview", ImVec2(-1.0f, -1.0f));
        }
        ImGui::End();
    }

    if (ImGui::Begin(kWindowConsole, nullptr, panelFlags)) {
        ImGui::TextUnformatted("F1: GUI表示切り替え");
        ImGui::TextUnformatted("F2: タイトルへ戻る");
        ImGui::TextUnformatted("タブをドラッグするとレイアウトを並べ替えできます。");
        ImGui::Separator();
        ImGui::TextWrapped("%s", editorStatusMessage_.c_str());
    }
    ImGui::End();

    if (ImGuiFileDialog::Instance()->Display("GameSceneOpenDialog", ImGuiWindowFlags_NoCollapse, ImVec2(760, 460))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            LoadSceneObjects(ImGuiFileDialog::Instance()->GetFilePathName().c_str());
        }
        ImGuiFileDialog::Instance()->Close();
    }
    if (ImGuiFileDialog::Instance()->Display("GameSceneSaveDialog", ImGuiWindowFlags_NoCollapse, ImVec2(760, 460))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            SaveSceneObjects(ImGuiFileDialog::Instance()->GetFilePathName().c_str());
        }
        ImGuiFileDialog::Instance()->Close();
    }
    if (ImGuiFileDialog::Instance()->Display("ProjectAssetDialog", ImGuiWindowFlags_NoCollapse, ImVec2(760, 460))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            editorStatusMessage_ =
                "選択中のアセット: " +
                ImGuiFileDialog::Instance()->GetFilePathName();
        }
        ImGuiFileDialog::Instance()->Close();
    }
}
void GameRuntime::DrawHud()
{
    if (!player_) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    Math::Vector2 hudMin{};
    Math::Vector2 hudSize{};
    GetEffectiveHudViewportRect(hudMin, hudSize);
    const ImVec2 origin(hudMin.x, hudMin.y);
    const ImVec2 drawSize(hudSize.x, hudSize.y);

    const int maxHp = (std::max)(GetPlayerMaxHp(), 1);
    const int hp = (std::clamp)(GetPlayerHp(), 0, maxHp);
    const float hpRate =
        static_cast<float>(hp) / static_cast<float>(maxHp);
    const float chargeRate = (std::clamp)(
        static_cast<float>(chargeTimer_) /
        static_cast<float>((std::max)(chargeShotThreshold_, 1)),
        0.0f,
        1.0f);
    const bool isChargeReady = chargeTimer_ >= chargeShotThreshold_;
    auto drawBar = [drawList](
                       const ImVec2& min,
                       const ImVec2& max,
                       float rate,
                       ImU32 fillColor) {
        const ImVec2 fillMax(
            min.x + (max.x - min.x) * (std::clamp)(rate, 0.0f, 1.0f),
            max.y);
        drawList->AddRectFilled(min, max, IM_COL32(17, 24, 38, 225), 4.0f);
        drawList->AddRectFilled(min, fillMax, fillColor, 4.0f);
        drawList->AddRect(min, max, IM_COL32(228, 242, 255, 95), 4.0f);
    };

    if (hpRate <= 0.34f && !isGameOver_) {
        const int alertAlpha = static_cast<int>(
            28.0f + (0.34f - hpRate) * 170.0f +
            18.0f * (0.5f + 0.5f * std::sin(cameraTimer_ * 8.0f)));
        drawList->AddRectFilled(
            origin,
            ImVec2(origin.x + drawSize.x, origin.y + drawSize.y),
            IM_COL32(210, 34, 58, (std::clamp)(alertAlpha, 0, 86)));
    }

    const ImVec2 playerPanelMin(origin.x + 18.0f, origin.y + 18.0f);
    const ImVec2 playerPanelMax(playerPanelMin.x + 300.0f, playerPanelMin.y + 116.0f);
    const ImVec2 hpBarMin(playerPanelMin.x + 18.0f, playerPanelMin.y + 48.0f);
    const ImVec2 hpBarMax(hpBarMin.x + 244.0f, hpBarMin.y + 15.0f);
    const ImVec2 chargeBarMin(playerPanelMin.x + 18.0f, playerPanelMin.y + 84.0f);
    const ImVec2 chargeBarMax(chargeBarMin.x + 244.0f, chargeBarMin.y + 10.0f);
    const std::string hpText = std::to_string(hp) + " / " + std::to_string(maxHp);
    const ImU32 hpColor =
        hpRate < 0.3f ? IM_COL32(255, 86, 94, 245) :
        hpRate < 0.55f ? IM_COL32(255, 205, 88, 245) :
                         IM_COL32(86, 232, 148, 245);

    drawList->AddRectFilled(
        ImVec2(playerPanelMin.x + 4.0f, playerPanelMin.y + 5.0f),
        ImVec2(playerPanelMax.x + 4.0f, playerPanelMax.y + 5.0f),
        IM_COL32(0, 0, 0, 80),
        6.0f);
    drawList->AddRectFilled(
        playerPanelMin,
        playerPanelMax,
        IM_COL32(8, 13, 25, 188),
        6.0f);
    drawList->AddRect(
        playerPanelMin,
        playerPanelMax,
        IM_COL32(110, 178, 232, 95),
        6.0f);
    drawList->AddRectFilled(
        playerPanelMin,
        ImVec2(playerPanelMax.x, playerPanelMin.y + 4.0f),
        IM_COL32(104, 225, 255, 210),
        6.0f);
    drawList->AddText(
        ImVec2(playerPanelMin.x + 16.0f, playerPanelMin.y + 14.0f),
        IM_COL32(232, 244, 255, 245),
        "プレイヤー");
    drawList->AddText(
        ImVec2(playerPanelMax.x - 74.0f, playerPanelMin.y + 14.0f),
        IM_COL32(168, 222, 255, 225),
        hasLockTarget_ ? "補助" : "照準");
    drawList->AddText(
        ImVec2(hpBarMin.x, hpBarMin.y - 18.0f),
        IM_COL32(196, 214, 232, 215),
        "HP");
    drawList->AddText(
        ImVec2(hpBarMax.x - ImGui::CalcTextSize(hpText.c_str()).x, hpBarMin.y - 18.0f),
        IM_COL32(232, 244, 255, 235),
        hpText.c_str());
    drawBar(hpBarMin, hpBarMax, hpRate, hpColor);

    drawList->AddText(
        ImVec2(chargeBarMin.x, chargeBarMin.y - 18.0f),
        IM_COL32(196, 214, 232, 215),
        "チャージ");
    const char* chargeStatusText = isChargeReady ? "準備完了" : "蓄積中";
    const ImVec2 chargeStatusSize = ImGui::CalcTextSize(chargeStatusText);
    drawList->AddText(
        ImVec2(chargeBarMax.x - chargeStatusSize.x, chargeBarMin.y - 18.0f),
        isChargeReady ? IM_COL32(116, 242, 255, 245) :
                        IM_COL32(198, 214, 232, 190),
        chargeStatusText);
    drawBar(
        chargeBarMin,
        chargeBarMax,
        chargeRate,
        isChargeReady ? IM_COL32(112, 232, 255, 245) :
                        IM_COL32(248, 205, 82, 230));
    if (isChargeReady) {
        drawList->AddRect(
            ImVec2(chargeBarMin.x - 2.0f, chargeBarMin.y - 2.0f),
            ImVec2(chargeBarMax.x + 2.0f, chargeBarMax.y + 2.0f),
            IM_COL32(154, 248, 255, 120),
            5.0f,
            0,
            2.0f);
    }

    const ImVec2 scorePanelMax(
        origin.x + drawSize.x - 18.0f,
        origin.y + 90.0f);
    const ImVec2 scorePanelMin(scorePanelMax.x - 242.0f, origin.y + 18.0f);
    const int waveNumber = currentWaveIndex_ < kWaveCount ? currentWaveIndex_ + 1 : kWaveCount;
    const int waveEnemyCount =
        currentWaveIndex_ < kWaveCount ? waveTuning_[currentWaveIndex_].enemyCount : 0;
    const std::string scoreText = std::to_string(score_);
    const std::string waveText =
        "ウェーブ " + std::to_string(waveNumber) + " / " + std::to_string(kWaveCount);
    const std::string enemyText =
        "敵 " + std::to_string(enemies_.size()) + "  出現 " +
        std::to_string(currentWaveIndex_ < kWaveCount ? spawnedEnemyCountInWave_ : waveEnemyCount) +
        " / " + std::to_string(waveEnemyCount);

    drawList->AddRectFilled(
        ImVec2(scorePanelMin.x + 4.0f, scorePanelMin.y + 5.0f),
        ImVec2(scorePanelMax.x + 4.0f, scorePanelMax.y + 5.0f),
        IM_COL32(0, 0, 0, 72),
        6.0f);
    drawList->AddRectFilled(
        scorePanelMin,
        scorePanelMax,
        IM_COL32(8, 13, 25, 170),
        6.0f);
    drawList->AddRect(
        scorePanelMin,
        scorePanelMax,
        IM_COL32(245, 219, 126, 100),
        6.0f);
    drawList->AddText(
        ImVec2(scorePanelMin.x + 14.0f, scorePanelMin.y + 10.0f),
        IM_COL32(248, 225, 128, 240),
        "スコア");
    drawList->AddText(
        ImVec2(scorePanelMax.x - 16.0f - ImGui::CalcTextSize(scoreText.c_str()).x,
               scorePanelMin.y + 10.0f),
        IM_COL32(255, 246, 185, 255),
        scoreText.c_str());
    drawList->AddText(
        ImVec2(scorePanelMin.x + 14.0f, scorePanelMin.y + 36.0f),
        IM_COL32(205, 224, 242, 220),
        waveText.c_str());
    drawList->AddText(
        ImVec2(scorePanelMin.x + 14.0f, scorePanelMin.y + 54.0f),
        IM_COL32(180, 202, 222, 205),
        enemyText.c_str());

    DrawHitEffects();
    DrawLockOnHud();
}

void GameRuntime::DrawLockOnHud()
{
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const bool targetAligned = isReticleOnTarget_;
    const bool isChargeReady = chargeTimer_ >= chargeShotThreshold_;
    const ImU32 reticleColor =
        targetAligned ? IM_COL32(255, 66, 70, 255) : IM_COL32(80, 255, 150, 230);
    const ImU32 reticleSoftColor =
        targetAligned ? IM_COL32(255, 48, 44, 105) : IM_COL32(58, 255, 145, 70);
    constexpr float kReticleSize = 18.0f;
    constexpr float kReticleGap = 5.0f;
    const float reticleThickness = targetAligned ? 3.0f : 2.0f;

    drawList->AddCircle(
        ImVec2(reticleScreen_.x, reticleScreen_.y),
        kReticleSize + (targetAligned ? 7.0f : 4.0f),
        reticleSoftColor,
        48,
        targetAligned ? 3.0f : 2.0f);

    if (isChargeReady) {
        drawList->AddCircle(
            ImVec2(reticleScreen_.x, reticleScreen_.y),
            kReticleSize + 10.0f,
            targetAligned ? IM_COL32(255, 84, 86, 210) : IM_COL32(112, 255, 185, 190),
            52,
            2.0f);
    }

    drawList->AddCircle(
        ImVec2(reticleScreen_.x, reticleScreen_.y),
        kReticleSize,
        reticleColor,
        40,
        reticleThickness);
    drawList->AddLine(
        ImVec2(reticleScreen_.x - kReticleSize - kReticleGap, reticleScreen_.y),
        ImVec2(reticleScreen_.x - kReticleGap, reticleScreen_.y),
        reticleColor,
        reticleThickness);
    drawList->AddLine(
        ImVec2(reticleScreen_.x + kReticleGap, reticleScreen_.y),
        ImVec2(reticleScreen_.x + kReticleSize + kReticleGap, reticleScreen_.y),
        reticleColor,
        reticleThickness);
    drawList->AddLine(
        ImVec2(reticleScreen_.x, reticleScreen_.y - kReticleSize - kReticleGap),
        ImVec2(reticleScreen_.x, reticleScreen_.y - kReticleGap),
        reticleColor,
        reticleThickness);
    drawList->AddLine(
        ImVec2(reticleScreen_.x, reticleScreen_.y + kReticleGap),
        ImVec2(reticleScreen_.x, reticleScreen_.y + kReticleSize + kReticleGap),
        reticleColor,
        reticleThickness);
}

void GameRuntime::DrawResultOverlay()
{
    if (!isGameOver_ && !isGameClear_) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    Math::Vector2 hudMin{};
    Math::Vector2 hudSize{};
    GetEffectiveHudViewportRect(hudMin, hudSize);
    const ImVec2 origin(hudMin.x, hudMin.y);
    const ImVec2 drawSize(hudSize.x, hudSize.y);
    const ImVec2 center(
        origin.x + drawSize.x * 0.5f,
        origin.y + drawSize.y * 0.44f);
    const ImVec2 panelSize(340.0f, 112.0f);
    const ImVec2 panelMin(
        center.x - panelSize.x * 0.5f,
        center.y - panelSize.y * 0.5f);
    const ImVec2 panelMax(
        center.x + panelSize.x * 0.5f,
        center.y + panelSize.y * 0.5f);

    const char* title = isGameClear_ ? "MISSION CLEAR" : "GAME OVER";
    const char* guide = "F2: タイトルへ戻る";
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
        if ((*iterator)->CanHome()) {
            if (const Enemy* target = FindHomingTargetForBullet(*(*iterator))) {
                (*iterator)->SetHomingTarget(target->GetTranslate());
            } else {
                (*iterator)->ClearHomingTarget();
            }
        }
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
            if ((*iterator)->HasEscaped() && spawnedEnemyCountInWave_ > defeatedEnemyCountInWave_) {
                --spawnedEnemyCountInWave_;
            }
            iterator = enemies_.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

Math::Vector3 GameRuntime::CalculateAimDirection(const Math::Vector3& origin) const
{
    if (!camera_) {
        return { 0.0f, 0.0f, 1.0f };
    }

    Math::Vector2 viewportMin{};
    Math::Vector2 viewportSize{};
    GetEffectiveHudViewportRect(viewportMin, viewportSize);
    if (viewportSize.x <= 1.0f || viewportSize.y <= 1.0f) {
        return { 0.0f, 0.0f, 1.0f };
    }

    const float ndcX =
        ((reticleScreen_.x - viewportMin.x) / viewportSize.x) * 2.0f - 1.0f;
    const float ndcY =
        1.0f - ((reticleScreen_.y - viewportMin.y) / viewportSize.y) * 2.0f;
    const Math::Matrix4x4 inverseViewProjection =
        Math::Inverse(camera_->GetViewProjectionMatrix());
    const Math::Vector3 nearPoint =
        TransformCoord({ ndcX, ndcY, 0.0f }, inverseViewProjection);
    const Math::Vector3 farPoint =
        TransformCoord({ ndcX, ndcY, 1.0f }, inverseViewProjection);
    Math::Vector3 direction = Math::Normalize({
        farPoint.x - nearPoint.x,
        farPoint.y - nearPoint.y,
        farPoint.z - nearPoint.z
    });
    if (std::abs(direction.x) + std::abs(direction.y) + std::abs(direction.z) <= 0.001f) {
        direction = Math::Normalize({
            nearPoint.x - origin.x,
            nearPoint.y - origin.y,
            nearPoint.z - origin.z
        });
    }
    if (std::abs(direction.x) + std::abs(direction.y) + std::abs(direction.z) <= 0.001f) {
        return { 0.0f, 0.0f, 1.0f };
    }
    return direction;
}

const Enemy* GameRuntime::FindHomingTargetForBullet(const Bullet& bullet) const
{
    const Enemy* bestEnemy = nullptr;
    float bestScore = 42.0f * 42.0f;
    const Math::Vector3 bulletPosition = bullet.GetTranslate();

    for (const auto& enemy : enemies_) {
        if (!enemy || enemy->IsDead()) {
            continue;
        }
        const Math::Vector3 enemyPosition = enemy->GetTranslate();
        if (enemyPosition.z + 2.0f < bulletPosition.z) {
            continue;
        }
        const float distanceSq = DistanceSquared(bulletPosition, enemyPosition);
        if (distanceSq < bestScore) {
            bestScore = distanceSq;
            bestEnemy = enemy.get();
        }
    }

    return bestEnemy;
}

int GameRuntime::GetTotalEnemyTargetCount() const
{
    int total = 0;
    for (const WaveTuning& wave : waveTuning_) {
        total += wave.enemyCount;
    }
    return total;
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
                const bool isChargedHit = bullet->GetRadius() >= 0.8f;
                const int damage = isChargedHit ? 3 : 1;
                AddEnemyImpactEffect(
                    bullet->GetTranslate(),
                    isChargedHit ? 1.16f : 1.0f);
                bullet->RegisterHit();
                const bool isDestroyed = enemy->Damage(damage);
                AddCameraShake(
                    isDestroyed ? (isChargedHit ? 0.075f : 0.055f) : 0.022f,
                    isDestroyed ? (isChargedHit ? 11 : 8) : 4);
                if (isDestroyed) {
                    AddEnemyHitEffect(
                        enemy->GetTranslate(),
                        isChargedHit ? 1.32f : 1.0f);
                    score_ += 100;
                    ++defeatedEnemyCount_;
                    ++defeatedEnemyCountInWave_;
                }
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
            if (player_->IsInvincible()) {
                continue;
            }
            bullet->Kill();
            player_->Damage(10);
            AddPlayerDamageEffect(player_->GetTranslate());
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
