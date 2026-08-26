#include "app/GameRuntime.h"

#include "engine/3d/ModelManager.h"
#include "engine/3d/Skybox.h"
#include "engine/3d/TextureManager.h"
#include "engine/base/DirectXCommon.h"
#include "engine/io/Input.h"
#include "engine/scene/LevelDataLoader.h"
#include "engine/scene/SceneSerializer.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <IconsFontAwesome6.h>
#include <ImGuiFileDialog.h>
#include <implot.h>
#include <imgui_node_editor.h>
#include <TextEditor.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <numbers>
#include <string>

namespace {

constexpr const char* kGameSceneFilePath = "resources/game_scene.json";
constexpr const char* kGameEnvironmentTexturePath =
    "resources/skybox/kloofendal_48d_partly_cloudy_puresky_4k_cube.dds";
constexpr const char* kFreePlayerModelPath = "free_models/kenney_space_kit/craft_speederA.glb";
constexpr const char* kFreeEnemyModelPath =
    "free_models/quaternius_sci_fi_essentials/Enemy_EyeDrone_Static.gltf";
constexpr const char* kEnemyFormationModelPath =
    "free_models/Spitfire-20260707T202640Z-3-001/Spitfire/glTF/Spitfire.gltf";
constexpr const char* kEnemyFormationTexturePath =
    "resources/free_models/Spitfire-20260707T202640Z-3-001/Spitfire/Textures/Spitfire_Red.dds";
constexpr const char* kEnemySwoopModelPath =
    "free_models/Striker-20260707T202647Z-3-001/Striker/glTF/Striker.gltf";
constexpr const char* kEnemySwoopTexturePath =
    "resources/free_models/Striker-20260707T202647Z-3-001/Striker/Textures/Striker_Purple.dds";
constexpr const char* kEnemyShooterModelPath =
    "free_models/Pancake-20260707T202659Z-3-001/Pancake/glTF/Pancake.gltf";
constexpr const char* kEnemyShooterTexturePath =
    "resources/free_models/Pancake-20260707T202659Z-3-001/Pancake/Textures/Pancake_Orange.dds";
constexpr const char* kEnemyHeavyModelPath =
    "free_models/Challenger-20260707T202702Z-3-001/Challenger/glTF/Challenger.gltf";
constexpr const char* kEnemyHeavyTexturePath =
    "resources/free_models/Challenger-20260707T202702Z-3-001/Challenger/Textures/Challenger_Blue.dds";
constexpr const char* kBossModelPath =
    "free_models/Imperial-20260707T180457Z-3-001/Imperial/glTF/Imperial.gltf";
constexpr const char* kBossTexturePath =
    "resources/free_models/Imperial-20260707T180457Z-3-001/Imperial/Textures/Imperial_Purple.dds";
constexpr const char* kCityStreet4LaneModelPath =
    "free_models/Downtown City MegaKit[Standard]/"
    "Exports/glTF (Godot)/Street_4Lane.gltf";
constexpr const char* kCityManholeCoverModelPath =
    "free_models/Downtown City MegaKit[Standard]/"
    "Exports/glTF (Godot)/Prop_ManholeCover.gltf";
constexpr const char* kCityDrainModelPath =
    "free_models/Downtown City MegaKit[Standard]/"
    "Exports/glTF (Godot)/Prop_Drain.gltf";
constexpr const char* kCityBuildingSmallModelPath =
    "free_models/Downtown City MegaKit[Standard]/"
    "Exports/glTF (Godot)/Building_Small_1.gltf";
constexpr const char* kCityBuildingMediumModelPath =
    "free_models/Downtown City MegaKit[Standard]/"
    "Exports/glTF (Godot)/Building_Medium_2_001.gltf";
constexpr const char* kCityBuildingLargeModelPath =
    "free_models/Downtown City MegaKit[Standard]/"
    "Exports/glTF (Godot)/Building_Large_2.gltf";
constexpr int kInitialPlayerBulletPoolCount = 24;
constexpr int kInitialEnemyBulletPoolCount = 24;
constexpr int kTargetPlayerBulletPoolCount = 24;
constexpr int kTargetEnemyBulletPoolCount = 24;
constexpr int kBulletPoolWarmupStartDelayFrames = 6;
constexpr int kBulletPoolWarmupIntervalFrames = 1;
constexpr int kInitialHitEffectObjectPoolCount = 120;
constexpr int kTargetHitEffectObjectPoolCount = 120;
constexpr int kHitEffectPoolWarmupStartDelayFrames = 6;
constexpr int kHitEffectPoolWarmupIntervalFrames = 1;
constexpr int kRewardHeartPoolCount = 40;
constexpr int kRewardHeartScoreValue = 25;
constexpr int kDepthCueEffectCount = 12;
constexpr float kDepthCueNearLocalZ = -10.0f;
constexpr float kDepthCueFarLocalZ = 148.0f;
constexpr float kDepthCueLoopLength = 172.0f;
constexpr float kContactShadowY = -2.82f;
constexpr float kContactShadowHalfPi = 1.57079632679f;
constexpr float kTwoPi = 6.28318530718f;
constexpr float kRailCameraCurveFrequency = 0.050f;
constexpr float kRailCameraDriftFrequency = 0.027f;
constexpr float kGameplayCameraBaseFovY = 0.590f;
constexpr float kGameplayCameraInitialDistance = 15.8f;
constexpr float kGameplayCameraBaseDistance = 16.10f;
constexpr float kGameplayCameraEdgeRollStart = 5.25f;
constexpr float kGameplayCameraEdgeRollRange = 3.65f;
constexpr float kGameplayCameraEdgeRollMax = 0.055f;
constexpr float kPlayerExhaustRiseResponse = 0.24f;
constexpr float kPlayerExhaustFallResponse = 0.075f;
constexpr float kPlayerExhaustParticleInterval = 1.0f / 38.0f;
constexpr float kJustDodgeCameraClosePushIn = 3.35f;
constexpr float kJustDodgeCameraPlayerFollowX = 0.115f;
constexpr float kJustDodgeCameraPlayerFollowY = 0.105f;
constexpr float kJustDodgeCameraLowAngle = 0.18f;
constexpr float kJustDodgeCameraFovTighten = 0.140f;
constexpr int kPlayerDodgeAfterimageIntervalFrames = 2;
constexpr float kPlayerDodgeAfterimageDuration = 34.0f;
constexpr int kSharedResourcePreloadStepCount = 15;
constexpr float kSceneryNearLocalZ = -24.0f;
constexpr float kSceneryFarLocalZ = 300.0f;
constexpr float kStageClearDistance = 365.0f;
constexpr float kBossWarningDistance = 300.0f;
constexpr float kBossSpawnDistance = 330.0f;
constexpr int kBossWarningDuration = 150;
constexpr int kBossIntroDuration = 120;
constexpr int kBossDefeatFlashDuration = 44;
constexpr int kBossMaxHp = 52;
constexpr int kMaxActiveStageEnemiesBeforeBoss = 7;
constexpr int kMaxStageEnemyEventsPerFrame = 2;
constexpr float kJustDodgeGrazePadding = 1.25f;
constexpr float kJustDodgeRailSlowScale = 0.08f;
constexpr int kJustDodgeChargeBonus = 24;
constexpr int kJustDodgeScoreBonus = 50;
constexpr int kJustDodgeFlashDuration = 46;
constexpr int kJustDodgeSlowDuration = 46;
constexpr int kPlayerImpactFlashDuration = 10;
constexpr int kPlayerImpactSlowDuration = 8;
constexpr float kPlayerImpactRailSlowScale = 0.42f;
int gSharedResourcePreloadStep = 0;
bool gSharedResourcesPreloaded = false;
const char* gSharedResourcePreloadLabel = "Waiting";
float gSharedResourceLastStepMs = 0.0f;
float gSharedResourceTotalMs = 0.0f;

Math::Vector3 RotateLocalOffset(
    const Math::Vector3& offset,
    const Math::Vector3& rotate)
{
    const Math::Matrix4x4 rotateMatrix =
        Math::Multiply(
            Math::MakeRotateXMatrix(rotate.x),
            Math::Multiply(
                Math::MakeRotateYMatrix(rotate.y),
                Math::MakeRotateZMatrix(rotate.z)));
    return {
        offset.x * rotateMatrix.m[0][0] +
            offset.y * rotateMatrix.m[1][0] +
            offset.z * rotateMatrix.m[2][0],
        offset.x * rotateMatrix.m[0][1] +
            offset.y * rotateMatrix.m[1][1] +
            offset.z * rotateMatrix.m[2][1],
        offset.x * rotateMatrix.m[0][2] +
            offset.y * rotateMatrix.m[1][2] +
            offset.z * rotateMatrix.m[2][2],
    };
}

struct EnemySpawnPattern {
    float x;
    float y;
    Enemy::Behavior behavior;
    Enemy::EntryStyle entryStyle;
};

struct StageSegment {
    float startDistance;
    float endDistance;
    float targetSpeed;
    float yawBias;
    float rollBias;
    float liftBias;
    float fovBoost;
    const char* name;
};

struct StageRailEvent {
    float distance;
    int spawnTimerCap;
    float shakePower;
    int shakeDuration;
};

struct StageEnemySpawnEvent {
    float distance;
    float x;
    float y;
    float leadDistance;
    Enemy::Behavior behavior;
    Enemy::EntryStyle entryStyle;
    float shakePower;
    int shakeDuration;
    int maxHpOverride;
    float scaleMultiplier;
    const char* beatName;
};

constexpr int kWaveCount = 3;

constexpr StageSegment kStageSegments[] = {
    {   0.0f,  42.0f, 0.145f,  0.000f,  0.000f,  0.00f, 0.042f, "Opening" },
    {  42.0f, 104.0f, 0.185f, -0.022f,  0.042f,  0.16f, 0.060f, "Side sweep" },
    { 104.0f, 170.0f, 0.165f,  0.018f, -0.036f,  0.34f, 0.054f, "Climb and dodge" },
    { 170.0f, 230.0f, 0.220f,  0.000f, -0.055f, -0.16f, 0.092f, "Fast dive" },
    { 230.0f, 999.0f, 0.178f,  0.018f,  0.034f,  0.08f, 0.060f, "Final approach" }
};

constexpr StageRailEvent kStageRailEvents[] = {
    {  12.0f,  0, 0.045f,  8 },
    {  54.0f,  3, 0.065f, 10 },
    { 112.0f,  2, 0.058f,  8 },
    { 176.0f,  1, 0.080f, 12 },
    { 246.0f,  0, 0.066f, 10 }
};

constexpr StageEnemySpawnEvent kStageEnemySpawnEvents[] = {
    {  10.0f, -2.7f, -0.5f, 42.0f, Enemy::Behavior::Formation,     Enemy::EntryStyle::VFormation, 0.020f,  4,  4, 1.08f, "Opening pair" },
    {  16.0f,  2.7f,  0.2f, 42.0f, Enemy::Behavior::Formation,     Enemy::EntryStyle::VFormation, 0.020f,  4,  4, 1.08f, "Opening pair" },
    {  28.0f,  0.0f,  1.4f, 50.0f, Enemy::Behavior::DiveBomber,    Enemy::EntryStyle::Direct,     0.030f,  7,  5, 1.08f, "Center dive" },
    {  38.0f,  5.1f,  1.2f, 48.0f, Enemy::Behavior::Swoop,         Enemy::EntryStyle::RightSweep, 0.032f,  6,  5, 1.08f, "Right sweep" },
    {  58.0f, -5.1f,  0.7f, 48.0f, Enemy::Behavior::Swoop,         Enemy::EntryStyle::LeftSweep,  0.032f,  6,  5, 1.08f, "Cross sweep" },
    {  68.0f, -4.0f, -0.6f, 44.0f, Enemy::Behavior::Formation,     Enemy::EntryStyle::VFormation, 0.022f,  5,  4, 1.04f, "Low gate" },
    {  78.0f, -3.2f,  1.3f, 46.0f, Enemy::Behavior::StrafeShooter, Enemy::EntryStyle::PopShooter, 0.042f,  8,  7, 1.16f, "Dodge target" },
    {  98.0f,  3.2f,  0.9f, 46.0f, Enemy::Behavior::StrafeShooter, Enemy::EntryStyle::PopShooter, 0.042f,  8,  7, 1.16f, "Dodge target" },
    { 112.0f,  4.6f,  1.6f, 54.0f, Enemy::Behavior::DiveBomber,    Enemy::EntryStyle::RightSweep, 0.036f,  8,  5, 1.12f, "High dive" },
    { 124.0f, -2.9f, -0.4f, 44.0f, Enemy::Behavior::Formation,     Enemy::EntryStyle::VFormation, 0.024f,  5,  5, 1.12f, "Guard pair" },
    { 134.0f,  2.9f, -0.2f, 44.0f, Enemy::Behavior::Formation,     Enemy::EntryStyle::VFormation, 0.024f,  5,  5, 1.12f, "Guard pair" },
    { 148.0f, -4.6f,  1.5f, 54.0f, Enemy::Behavior::DiveBomber,    Enemy::EntryStyle::LeftSweep,  0.036f,  8,  5, 1.12f, "High dive" },
    { 160.0f,  0.0f,  1.45f, 52.0f, Enemy::Behavior::StrafeShooter, Enemy::EntryStyle::PopShooter, 0.070f, 14, 12, 1.48f, "Heavy drone" },
    { 174.0f, -5.4f,  0.55f, 52.0f, Enemy::Behavior::Crossfire,     Enemy::EntryStyle::LeftSweep,  0.048f,  9,  6, 1.12f, "Crossfire pair" },
    { 174.0f,  5.4f,  1.15f, 52.0f, Enemy::Behavior::Crossfire,     Enemy::EntryStyle::RightSweep, 0.048f,  9,  6, 1.12f, "Crossfire pair" },
    { 188.0f, -4.9f,  1.0f, 52.0f, Enemy::Behavior::Swoop,         Enemy::EntryStyle::LeftSweep,  0.038f,  7,  5, 1.14f, "High-speed pass" },
    { 202.0f,  4.9f,  1.0f, 52.0f, Enemy::Behavior::Swoop,         Enemy::EntryStyle::RightSweep, 0.038f,  7,  5, 1.14f, "High-speed pass" },
    { 214.0f,  0.0f,  1.0f, 50.0f, Enemy::Behavior::Formation,     Enemy::EntryStyle::VFormation, 0.030f,  6,  5, 1.18f, "Break formation" },
    { 228.0f, -3.8f,  0.6f, 48.0f, Enemy::Behavior::StrafeShooter, Enemy::EntryStyle::PopShooter, 0.040f,  8,  7, 1.18f, "Final crossfire" },
    { 248.0f,  3.8f,  0.6f, 48.0f, Enemy::Behavior::StrafeShooter, Enemy::EntryStyle::PopShooter, 0.040f,  8,  7, 1.18f, "Final crossfire" },
    { 258.0f, -2.9f, -0.5f, 44.0f, Enemy::Behavior::Formation,     Enemy::EntryStyle::VFormation, 0.030f,  6,  5, 1.16f, "Last gate" },
    { 274.0f,  2.9f, -0.5f, 44.0f, Enemy::Behavior::Formation,     Enemy::EntryStyle::VFormation, 0.030f,  6,  5, 1.16f, "Last gate" },
    { 286.0f, -5.2f,  1.2f, 54.0f, Enemy::Behavior::Swoop,         Enemy::EntryStyle::LeftSweep,  0.034f,  7,  5, 1.12f, "Boss screen" },
    { 296.0f,  5.2f,  1.2f, 54.0f, Enemy::Behavior::Swoop,         Enemy::EntryStyle::RightSweep, 0.034f,  7,  5, 1.12f, "Boss screen" }
};

constexpr size_t kStageEnemyEventCapacity = 24;
constexpr size_t kStageEnemyEventCount =
    sizeof(kStageEnemySpawnEvents) / sizeof(kStageEnemySpawnEvents[0]);
static_assert(kStageEnemyEventCount <= kStageEnemyEventCapacity);

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

float PseudoRandom01(int index, float salt)
{
    const float value =
        std::sin((static_cast<float>(index) + 1.0f) * 12.9898f + salt * 78.233f) *
        43758.5453f;
    return value - std::floor(value);
}

float WrapSceneryLocalZ(float localZ, float loopLength)
{
    const float safeLoopLength = (std::max)(loopLength, 1.0f);
    while (localZ < kSceneryNearLocalZ) {
        localZ += safeLoopLength;
    }
    while (localZ > kSceneryFarLocalZ) {
        localZ -= safeLoopLength;
    }
    return localZ;
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

bool GameRuntime::PreloadSharedResourceStep(
    DirectXCommon* dxCommon,
    SrvManager* srvManager)
{
    if (gSharedResourcesPreloaded) {
        gSharedResourcePreloadLabel = "Ready";
        return true;
    }
    if (!dxCommon || !srvManager) {
        gSharedResourcePreloadLabel = "Waiting for renderer";
        return false;
    }

    ModelManager* modelManager = ModelManager::GetInstance();
    const auto stepBegin = std::chrono::steady_clock::now();
    dxCommon->BeginTextureUploadBatch();
    switch (gSharedResourcePreloadStep) {
    case 0:
        gSharedResourcePreloadLabel = "Renderer resources";
        modelManager->Initialize(dxCommon, srvManager);
        modelManager->SetEnvironmentTexturePath(kGameEnvironmentTexturePath);
        break;
    case 1:
        gSharedResourcePreloadLabel = "Core game meshes";
        modelManager->CreateBox("game_player", 1.0f, 0.45f, 1.3f, "resources/uvChecker.png");
        modelManager->CreateSphere("game_bullet", 12, 24, 0.35f, "resources/human/white.png");
        modelManager->CreateSphere("game_enemy", 16, 32, 0.9f, "resources/uvChecker.png");
        modelManager->CreatePlane("primitive_plane", 8.0f, 8.0f, "resources/checkerBoard.png");
        modelManager->CreateTriangle("primitive_triangle", 1.6f, 1.6f, "resources/uvChecker.png");
        modelManager->CreateCircle("primitive_circle", 32, 0.9f, "resources/uvChecker.png");
        break;
    case 2:
        gSharedResourcePreloadLabel = "Reward and glow meshes";
        modelManager->CreateHeart("reward_heart", 48, 1.0f, "resources/human/white.png");
        modelManager->CreateCircle("effect_glow_core", 48, 1.0f, "resources/effects/glow_core.png");
        modelManager->CreateCircle("effect_glow_ring", 64, 1.0f, "resources/effects/glow_ring.png");
        modelManager->CreateCircle("effect_spark_star", 48, 1.0f, "resources/effects/pal_star_spark.png");
        modelManager->CreateCircle("effect_bullet_glow", 48, 1.0f, "resources/effects/glow_core.png");
        modelManager->CreateCircle("effect_contact_shadow", 64, 1.0f, "resources/effects/glow_core.png");
        break;
    case 3:
        gSharedResourcePreloadLabel = "Projectile effect meshes";
        modelManager->CreatePlane("effect_bullet_trail", 1.0f, 1.0f, "resources/effects/bullet_trail.png");
        modelManager->CreatePlane("effect_player_bullet_core", 1.0f, 1.0f, "resources/effects/rail_player_bolt_core.png");
        modelManager->CreatePlane("effect_player_bullet_trail", 1.0f, 1.0f, "resources/effects/rail_player_bolt_trail.png");
        modelManager->CreatePlane("effect_player_charge_core", 1.0f, 1.0f, "resources/effects/rail_charge_lance_core.png");
        modelManager->CreatePlane("effect_player_charge_trail", 1.0f, 1.0f, "resources/effects/rail_charge_lance_trail.png");
        modelManager->CreatePlane("effect_enemy_bullet_core", 1.0f, 1.0f, "resources/effects/rail_enemy_orb_core.png");
        modelManager->CreatePlane("effect_enemy_bullet_tail", 1.0f, 1.0f, "resources/effects/rail_enemy_tail.png");
        modelManager->CreatePlane("effect_impact_burst", 1.0f, 1.0f, "resources/effects/pal_impact_burst.png");
        modelManager->CreatePlane("effect_magic_shard", 1.0f, 1.0f, "resources/effects/pal_magic_shard.png");
        modelManager->CreatePlane("effect_explosion_fireball", 1.0f, 1.0f, "resources/effects/rail_explosion_fireball.png");
        modelManager->CreatePlane("effect_explosion_smoke", 1.0f, 1.0f, "resources/effects/rail_explosion_smoke.png");
        modelManager->CreatePlane("effect_explosion_sparks", 1.0f, 1.0f, "resources/effects/rail_explosion_sparks.png");
        break;
    case 4:
        gSharedResourcePreloadLabel = "Primitive scene meshes";
        modelManager->CreateRing("primitive_ring", 32, 2.0f, 1.0f, "resources/gradationLine.png");
        modelManager->CreateSphere("primitive_sphere", 16, 32, 1.0f, "resources/uvChecker.png");
        modelManager->CreateTorus("primitive_torus", 32, 16, 0.8f, 0.3f, "resources/uvChecker.png");
        modelManager->CreateCylinder("primitive_cylinder", 32, 1.2f, 1.2f, 2.5f, "resources/gradationLine.png");
        modelManager->CreateCone("primitive_cone", 32, 0.8f, 1.6f, "resources/uvChecker.png");
        modelManager->CreateBox("primitive_box", 1.5f, 1.5f, 1.5f, "resources/uvChecker.png");
        break;
    case 5:
        gSharedResourcePreloadLabel = "Environment texture";
        TextureManager::GetInstance()->LoadTexture(kGameEnvironmentTexturePath);
        break;
    case 6:
        gSharedResourcePreloadLabel = "Gameplay shaders";
        dxCommon->CompileShader(L"shaders/Object3D.VS.hlsl", L"vs_6_0");
        dxCommon->CompileShader(L"shaders/Object3D.PS.hlsl", L"ps_6_0");
        dxCommon->CompileShader(L"shaders/Object3DShadow.VS.hlsl", L"vs_6_0");
        dxCommon->CompileShader(L"shaders/Skybox.VS.hlsl", L"vs_6_0");
        dxCommon->CompileShader(L"shaders/Skybox.PS.hlsl", L"ps_6_0");
        break;
    case 7:
        gSharedResourcePreloadLabel = "Player ship model";
        modelManager->LoadModel(kFreePlayerModelPath);
        break;
    case 8:
        gSharedResourcePreloadLabel = "Enemy ship models";
        modelManager->LoadModel(kEnemyFormationModelPath);
        modelManager->LoadModel(kEnemySwoopModelPath);
        modelManager->LoadModel(kEnemyShooterModelPath);
        modelManager->LoadModel(kEnemyHeavyModelPath);
        modelManager->LoadModel(kFreeEnemyModelPath);
        TextureManager::GetInstance()->LoadTexture(kEnemyFormationTexturePath);
        TextureManager::GetInstance()->LoadTexture(kEnemySwoopTexturePath);
        TextureManager::GetInstance()->LoadTexture(kEnemyShooterTexturePath);
        TextureManager::GetInstance()->LoadTexture(kEnemyHeavyTexturePath);
        TextureManager::GetInstance()->LoadTexture(kBossTexturePath);
        break;
    case 9:
        gSharedResourcePreloadLabel = "City road model";
        modelManager->LoadModel(kCityStreet4LaneModelPath);
        break;
    case 10:
        gSharedResourcePreloadLabel = "City street detail models";
        modelManager->LoadModel(kCityManholeCoverModelPath);
        modelManager->LoadModel(kCityDrainModelPath);
        break;
    case 11:
        gSharedResourcePreloadLabel = "City building small model";
        modelManager->LoadModel(kCityBuildingSmallModelPath);
        break;
    case 12:
        gSharedResourcePreloadLabel = "City building medium model";
        modelManager->LoadModel(kCityBuildingMediumModelPath);
        break;
    case 13:
        gSharedResourcePreloadLabel = "City building large model";
        modelManager->LoadModel(kCityBuildingLargeModelPath);
        break;
    case 14:
        gSharedResourcePreloadLabel = "Boss spaceship model";
        modelManager->LoadModel(kBossModelPath);
        break;
    default:
        gSharedResourcesPreloaded = true;
        gSharedResourcePreloadLabel = "Ready";
        return true;
    }
    dxCommon->EndTextureUploadBatch();

    const auto stepEnd = std::chrono::steady_clock::now();
    gSharedResourceLastStepMs =
        std::chrono::duration<float, std::milli>(stepEnd - stepBegin).count();
    gSharedResourceTotalMs += gSharedResourceLastStepMs;

    ++gSharedResourcePreloadStep;
    if (gSharedResourcePreloadStep >= kSharedResourcePreloadStepCount) {
        gSharedResourcesPreloaded = true;
        gSharedResourcePreloadLabel = "Ready";
    }
    return gSharedResourcesPreloaded;
}

bool GameRuntime::AreSharedResourcesPreloaded()
{
    return gSharedResourcesPreloaded;
}

int GameRuntime::GetSharedResourcePreloadStep()
{
    return std::clamp(
        gSharedResourcePreloadStep,
        0,
        kSharedResourcePreloadStepCount);
}

int GameRuntime::GetSharedResourcePreloadStepCount()
{
    return kSharedResourcePreloadStepCount;
}

const char* GameRuntime::GetSharedResourcePreloadLabel()
{
    return gSharedResourcePreloadLabel;
}

float GameRuntime::GetSharedResourceLastStepMs()
{
    return gSharedResourceLastStepMs;
}

float GameRuntime::GetSharedResourceTotalMs()
{
    return gSharedResourceTotalMs;
}

void GameRuntime::Initialize()
{
    isExitRequested_ = false;
    isGameClear_ = false;
    isGameOver_ = false;
    bossWarningTriggered_ = false;
    bossSpawned_ = false;
    bossDefeated_ = false;
    isPerformanceOverlayVisible_ = false;
    showSkybox_ = true;
    currentWaveIndex_ = 0;
    spawnedEnemyCountInWave_ = 0;
    defeatedEnemyCountInWave_ = 0;
    spawnSequenceIndex_ = 0;
    defeatedEnemyCount_ = 0;
    score_ = 0;
    enemySpawnTimer_ = 0;
    enemyShotTimer_ = 32;
    bossWarningTimer_ = 0;
    bossIntroTimer_ = 0;
    bossDefeatFlashTimer_ = 0;
    resultTransitionTimer_ = -1;
    railDistance_ = 0.0f;
    railSpeed_ = 0.145f;
    targetRailSpeed_ = 0.145f;
    stageCameraYawBias_ = 0.0f;
    stageCameraRollBias_ = 0.0f;
    stageCameraLiftBias_ = 0.0f;
    stageCameraFovBoost_ = 0.0f;
    playerExhaustThrust_ = 0.0f;
    playerExhaustParticleTimer_ = 0.0f;
    nextPlayerExhaustParticleIndex_ = 0;
    stageSectionName_ = "Opening";
    stageCombatBeatName_ = "Intro";
    stageRailEventTriggered_.fill(false);
    stageEnemyEventTriggered_.fill(false);
    shootCooldown_ = 0;
    shootBufferTimer_ = 0;
    chargeTimer_ = 0;
    chargeFlashTimer_ = 0;
    playerDodgeAfterimageTimer_ = 0;
    justDodgeFlashTimer_ = 0;
    justDodgeSlowTimer_ = 0;
    playerImpactFlashTimer_ = 0;
    playerImpactFlashDuration_ = 1;
    playerImpactSlowTimer_ = 0;
    playerImpactSlowDuration_ = 1;
    playerImpactSlowScale_ = 1.0f;
    nextPlayerDodgeAfterimageIndex_ = 0;
    wasPlayerDodging_ = false;
    cameraShakeTimer_ = 0;
    bulletPoolWarmupTimer_ = 0;
    playerBulletPoolMisses_ = 0;
    enemyBulletPoolMisses_ = 0;
    hitEffectObjectPoolMisses_ = 0;
    rewardHeartPoolMisses_ = 0;
    maxActivePlayerBullets_ = 0;
    maxActiveEnemyBullets_ = 0;
    justDodgedEnemyBullets_.clear();
    hitEffects_.clear();
    hitEffects_.reserve(32);
    hitEffectObjectPool_.clear();

    object3dCommon_ = std::make_unique<Object3dCommon>();
    object3dCommon_->Initialize(dxCommon_, srvManager_);
    object3dCommon_->SetEnvironmentTexturePath(kGameEnvironmentTexturePath);

    while (!PreloadSharedResourceStep(dxCommon_, srvManager_)) {
    }

    playerModel_ = ModelManager::GetInstance()->FindModel(kFreePlayerModelPath);
    if (!playerModel_) {
        playerModel_ = ModelManager::GetInstance()->FindModel("game_player");
    }
    bulletModel_ = ModelManager::GetInstance()->FindModel("game_bullet");
    enemyModel_ = ModelManager::GetInstance()->FindModel(kFreeEnemyModelPath);
    if (!enemyModel_) {
        enemyModel_ = ModelManager::GetInstance()->FindModel("game_enemy");
    }
    enemyFormationModel_ = ModelManager::GetInstance()->FindModel(kEnemyFormationModelPath);
    enemySwoopModel_ = ModelManager::GetInstance()->FindModel(kEnemySwoopModelPath);
    enemyShooterModel_ = ModelManager::GetInstance()->FindModel(kEnemyShooterModelPath);
    enemyHeavyModel_ = ModelManager::GetInstance()->FindModel(kEnemyHeavyModelPath);
    bossModel_ = ModelManager::GetInstance()->FindModel(kBossModelPath);
    if (!bossModel_) {
        bossModel_ = enemyModel_;
    }
    effectGlowCoreModel_ = ModelManager::GetInstance()->FindModel("effect_glow_core");
    effectGlowRingModel_ = ModelManager::GetInstance()->FindModel("effect_glow_ring");
    effectSparkStarModel_ = ModelManager::GetInstance()->FindModel("effect_spark_star");
    effectBulletGlowModel_ = ModelManager::GetInstance()->FindModel("effect_bullet_glow");
    effectBulletTrailModel_ = ModelManager::GetInstance()->FindModel("effect_bullet_trail");
    effectPlayerBulletCoreModel_ = ModelManager::GetInstance()->FindModel("effect_player_bullet_core");
    effectPlayerBulletTrailModel_ = ModelManager::GetInstance()->FindModel("effect_player_bullet_trail");
    effectPlayerChargeCoreModel_ = ModelManager::GetInstance()->FindModel("effect_player_charge_core");
    effectPlayerChargeTrailModel_ = ModelManager::GetInstance()->FindModel("effect_player_charge_trail");
    effectEnemyBulletCoreModel_ = ModelManager::GetInstance()->FindModel("effect_enemy_bullet_core");
    effectEnemyBulletTailModel_ = ModelManager::GetInstance()->FindModel("effect_enemy_bullet_tail");
    effectImpactBurstModel_ = ModelManager::GetInstance()->FindModel("effect_impact_burst");
    effectMagicShardModel_ = ModelManager::GetInstance()->FindModel("effect_magic_shard");
    effectExplosionFireballModel_ = ModelManager::GetInstance()->FindModel("effect_explosion_fireball");
    effectExplosionSmokeModel_ = ModelManager::GetInstance()->FindModel("effect_explosion_smoke");
    effectExplosionSparksModel_ = ModelManager::GetInstance()->FindModel("effect_explosion_sparks");
    effectContactShadowModel_ = ModelManager::GetInstance()->FindModel("effect_contact_shadow");

    camera_ = std::make_unique<Camera>();
    camera_->SetRotate({ 0.06f, 0.0f, 0.0f });
    camera_->SetTranslate({ 0.0f, 2.65f, -kGameplayCameraInitialDistance });
    camera_->SetFovY(kGameplayCameraBaseFovY);
    camera_->SetFarClip(520.0f);
    camera_->Update();
    object3dCommon_->SetDefaultCamera(camera_.get());

    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(dxCommon_, srvManager_, kGameEnvironmentTexturePath);
    skybox_->Update(camera_.get());

    player_ = std::make_unique<Player>();
    player_->Initialize(object3dCommon_.get(), playerModel_);
    player_->SetRailZ(railDistance_);
    previousPlayerTranslate_ = player_->GetTranslate();
    const Math::Vector3 initialPlayerTranslate = player_->GetTranslate();
    cameraTranslate_ = {
        initialPlayerTranslate.x * 0.10f,
        2.70f + initialPlayerTranslate.y * 0.05f,
        railDistance_ - kGameplayCameraInitialDistance,
    };
    cameraRotate_ = {
        0.065f + initialPlayerTranslate.y * 0.003f,
        -initialPlayerTranslate.x * 0.004f,
        0.0f,
    };
    cameraFovY_ = kGameplayCameraBaseFovY;
    camera_->SetTranslate(cameraTranslate_);
    camera_->SetRotate(cameraRotate_);
    camera_->SetFovY(cameraFovY_);
    camera_->Update();
    if (skybox_) {
        skybox_->Update(camera_.get());
    }

    PrewarmBulletPools();
    PrewarmHitEffectObjectPool();
    InitializeRailScenery();
    InitializeDepthCueEffects();

    LoadSceneObjects(kGameSceneFilePath);
    InitializeRewardHearts();
    InitializePlayerDodgeAfterimages();
    InitializePlayerFlightAura();
    InitializePlayerExhaustParticles();
    InitializeContactShadows();
}

void GameRuntime::Finalize()
{
    sceneObjects_.clear();
    rewardHearts_.clear();
    for (PlayerDodgeAfterimage& afterimage : playerDodgeAfterimages_) {
        afterimage.object.reset();
        afterimage.isActive = false;
    }
    for (PlayerFlightAura& aura : playerFlightAuras_) {
        aura.object.reset();
    }
    for (PlayerExhaustParticle& particle : playerExhaustParticles_) {
        particle.object.reset();
        particle.isActive = false;
    }
    for (ContactShadow& shadow : contactShadows_) {
        shadow.object.reset();
    }
    playerBullets_.clear();
    enemyBullets_.clear();
    playerBulletPool_.clear();
    enemyBulletPool_.clear();
    homingBulletTargets_.clear();
    justDodgedEnemyBullets_.clear();
    enemies_.clear();
    hitEffects_.clear();
    hitEffectObjectPool_.clear();
    railSceneryObjects_.clear();
    depthCueEffects_.clear();
    player_.reset();
    camera_.reset();
    skybox_.reset();
    object3dCommon_.reset();

    playerModel_ = nullptr;
    bulletModel_ = nullptr;
    enemyModel_ = nullptr;
    enemyFormationModel_ = nullptr;
    enemySwoopModel_ = nullptr;
    enemyShooterModel_ = nullptr;
    enemyHeavyModel_ = nullptr;
    bossModel_ = nullptr;
    effectGlowCoreModel_ = nullptr;
    effectGlowRingModel_ = nullptr;
    effectSparkStarModel_ = nullptr;
    effectBulletGlowModel_ = nullptr;
    effectBulletTrailModel_ = nullptr;
    effectPlayerBulletCoreModel_ = nullptr;
    effectPlayerBulletTrailModel_ = nullptr;
    effectPlayerChargeCoreModel_ = nullptr;
    effectPlayerChargeTrailModel_ = nullptr;
    effectEnemyBulletCoreModel_ = nullptr;
    effectEnemyBulletTailModel_ = nullptr;
    effectImpactBurstModel_ = nullptr;
    effectMagicShardModel_ = nullptr;
    effectExplosionFireballModel_ = nullptr;
    effectExplosionSmokeModel_ = nullptr;
    effectExplosionSparksModel_ = nullptr;
    effectContactShadowModel_ = nullptr;

}

void GameRuntime::Update()
{
    if (HandleRuntimeShortcuts()) {
        return;
    }

    if (isBlenderLevelLoaded_) {
        // レールシューティングの進行を止め、Blender配置を確認しやすい固定視点にする。
        cameraTranslate_ = { 0.0f, 2.5f, -13.0f };
        cameraRotate_ = { 0.12f, 0.0f, 0.0f };
        cameraFovY_ = 0.60f;
        if (camera_) {
            camera_->SetTranslate(cameraTranslate_);
            camera_->SetRotate(cameraRotate_);
            camera_->SetFovY(cameraFovY_);
            camera_->Update();
        }
        if (skybox_) {
            skybox_->Update(camera_.get());
        }
        for (const auto& sceneObject : sceneObjects_) {
            if (sceneObject) {
                sceneObject->Update();
            }
        }
#ifdef ENABLE_DEBUG_GUI
        DrawEditorOverlayGuiRich();
#endif
        return;
    }

    UpdateBulletPoolWarmup();
    UpdateHitEffectObjectPoolWarmup();
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
    DrawPerformanceOverlay();
    DrawResultOverlay();
#ifdef ENABLE_DEBUG_GUI
    DrawEditorOverlayGuiRich();
#endif

    UpdateResultAndSceneObjects();
}

bool GameRuntime::HandleRuntimeShortcuts()
{
#ifdef ENABLE_DEBUG_GUI
    if (input_ && input_->TriggerKey(DIK_F1)) {
        isEditorOverlayVisible_ = !isEditorOverlayVisible_;
        return true;
    }
#endif
    if (input_ && input_->TriggerKey(DIK_F3)) {
        isPerformanceOverlayVisible_ = !isPerformanceOverlayVisible_;
        return true;
    }
    if (input_ && input_->TriggerKey(DIK_F4)) {
        isPostEffectBypassEnabled_ = !isPostEffectBypassEnabled_;
        return true;
    }
    if (input_ && input_->TriggerKey(DIK_F5)) {
        showSkybox_ = !showSkybox_;
        return true;
    }
    if (input_ && input_->TriggerKey(DIK_F2)) {
        isExitRequested_ = true;
        return true;
    }
    return false;
}

void GameRuntime::UpdateStageDirector()
{
    const StageSegment* activeSegment = &kStageSegments[0];
    for (const StageSegment& segment : kStageSegments) {
        if (railDistance_ >= segment.startDistance &&
            railDistance_ < segment.endDistance) {
            activeSegment = &segment;
            break;
        }
    }

    stageSectionName_ = activeSegment->name;
    currentWaveIndex_ =
        railDistance_ < 104.0f ? 0 :
        railDistance_ < 230.0f ? 1 :
        2;
    targetRailSpeed_ = activeSegment->targetSpeed;
    stageCameraYawBias_ =
        Lerp(stageCameraYawBias_, activeSegment->yawBias, 0.035f);
    stageCameraRollBias_ =
        Lerp(stageCameraRollBias_, activeSegment->rollBias, 0.035f);
    stageCameraLiftBias_ =
        Lerp(stageCameraLiftBias_, activeSegment->liftBias, 0.035f);
    stageCameraFovBoost_ =
        Lerp(stageCameraFovBoost_, activeSegment->fovBoost, 0.035f);

    const size_t eventCount =
        (std::min)(
            stageRailEventTriggered_.size(),
            sizeof(kStageRailEvents) / sizeof(kStageRailEvents[0]));
    for (size_t index = 0; index < eventCount; ++index) {
        const StageRailEvent& event = kStageRailEvents[index];
        if (stageRailEventTriggered_[index] ||
            railDistance_ < event.distance) {
            continue;
        }

        stageRailEventTriggered_[index] = true;
        if (!isGameOver_ && !isGameClear_ && currentWaveIndex_ < kWaveCount) {
            enemySpawnTimer_ = (std::min)(enemySpawnTimer_, event.spawnTimerCap);
        }
        AddCameraShake(event.shakePower, event.shakeDuration);
    }

    if (!bossWarningTriggered_ &&
        !bossSpawned_ &&
        !isGameOver_ &&
        !isGameClear_ &&
        railDistance_ >= kBossWarningDistance) {
        bossWarningTriggered_ = true;
        bossWarningTimer_ = kBossWarningDuration;
        stageCombatBeatName_ = "WARNING";
        AddCameraShake(0.13f, 28);
    }
}

void GameRuntime::UpdateRailProgress()
{
    if (!isGameOver_ && !isGameClear_) {
        UpdateStageDirector();
        railSpeed_ = Lerp(railSpeed_, targetRailSpeed_, 0.070f);
        railDistance_ += railSpeed_ * GetCinematicWorldTimeScale();
    }
}

void GameRuntime::UpdatePlayerAndCamera()
{
    player_->Update(input_, GetCinematicWorldTimeScale());
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
    if (shootBufferTimer_ > 0) {
        --shootBufferTimer_;
    }

    const bool isShootPressed = input_ && input_->PushKey(DIK_SPACE);
    const bool isShootTriggered = input_ && input_->TriggerKey(DIK_SPACE);
    if (isShootTriggered && shootCooldown_ > 0) {
        shootBufferTimer_ = 8;
    }

    const bool shouldShoot = isShootPressed || shootBufferTimer_ > 0;
    if (shouldShoot && shootCooldown_ <= 0) {
        const bool isCharged = chargeTimer_ >= chargeShotThreshold_;
        const bool hasAssistShot = isCharged && isReticleOnTarget_ && lockedEnemy_;
        FirePlayerBullet();
        shootBufferTimer_ = 0;
        AddCameraShake(
            isCharged ? (hasAssistShot ? 0.105f : 0.085f) : 0.026f,
            isCharged ? (hasAssistShot ? 16 : 14) : 5);
        shootCooldown_ =
            isCharged ? chargedShootCooldown_ : normalShootCooldown_;
    } else if (!isShootPressed && shootBufferTimer_ <= 0) {
        chargeTimer_ = (std::min)(chargeTimer_ + 1, kChargeShotMax);
    }

    if (chargeFlashTimer_ > 0) {
        --chargeFlashTimer_;
    }
}

void GameRuntime::UpdateEnemyActions()
{
    if (justDodgeSlowTimer_ > 0) {
        return;
    }

    UpdateEnemyWave();

    --enemyShotTimer_;
    if (enemyShotTimer_ <= 0) {
        int normalEnemyShotsThisVolley = 0;
        constexpr int kMaxNormalEnemyShotsThisVolley = 2;
        for (const auto& enemy : enemies_) {
            if (enemy->IsCrossfire() && enemy->CanShoot() &&
                normalEnemyShotsThisVolley < kMaxNormalEnemyShotsThisVolley) {
                FireEnemyBullet(enemy->GetAimPosition());
                ++normalEnemyShotsThisVolley;
            }
        }
        for (const auto& enemy : enemies_) {
            if (enemy->CanShoot()) {
                const Math::Vector3 enemyAimPosition = enemy->GetAimPosition();
                if (enemy->IsBoss()) {
                    FireEnemyBullet({
                        enemyAimPosition.x - 2.2f,
                        enemyAimPosition.y + 0.25f,
                        enemyAimPosition.z
                    });
                    FireEnemyBullet({
                        enemyAimPosition.x + 2.2f,
                        enemyAimPosition.y + 0.25f,
                        enemyAimPosition.z
                    });
                    if ((enemyShotTimer_ + static_cast<int>(railDistance_ * 10.0f)) % 2 == 0) {
                        FireEnemyBullet({
                            enemyAimPosition.x,
                            enemyAimPosition.y - 0.55f,
                            enemyAimPosition.z
                        });
                    }
                } else if (!enemy->IsCrossfire() &&
                    normalEnemyShotsThisVolley < kMaxNormalEnemyShotsThisVolley) {
                    FireEnemyBullet(enemyAimPosition);
                    ++normalEnemyShotsThisVolley;
                }
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
    UpdatePlayerDodgeAfterimages();
    UpdateRewardHearts();
    UpdateDepthCueEffects();
    UpdateRailScenery();
}

void GameRuntime::UpdateGameplayCollisions()
{
    CheckBulletEnemyCollisions();
    CheckEnemyBulletPlayerCollisions();
}

void GameRuntime::UpdateResultAndSceneObjects()
{
    if (bossWarningTimer_ > 0) {
        --bossWarningTimer_;
    }
    if (bossIntroTimer_ > 0) {
        --bossIntroTimer_;
    }
    if (bossDefeatFlashTimer_ > 0) {
        --bossDefeatFlashTimer_;
    }
    if (justDodgeFlashTimer_ > 0) {
        --justDodgeFlashTimer_;
    }
    if (justDodgeSlowTimer_ > 0) {
        --justDodgeSlowTimer_;
    }
    if (playerImpactFlashTimer_ > 0) {
        --playerImpactFlashTimer_;
        if (playerImpactFlashTimer_ <= 0) {
            playerImpactFlashDuration_ = 1;
        }
    }
    if (playerImpactSlowTimer_ > 0) {
        --playerImpactSlowTimer_;
        if (playerImpactSlowTimer_ <= 0) {
            playerImpactSlowDuration_ = 1;
            playerImpactSlowScale_ = 1.0f;
        }
    }
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

const Math::Matrix4x4& GameRuntime::GetProjectionMatrix() const
{
    static const Math::Matrix4x4 kIdentity = Math::MakeIdentity4x4();
    return camera_ ? camera_->GetProjectionMatrix() : kIdentity;
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

void GameRuntime::InitializeRewardHearts()
{
    rewardHearts_.clear();

    Model* heartModel =
        ModelManager::GetInstance()->FindModel("reward_heart");
    if (!heartModel || !object3dCommon_) {
        return;
    }

    rewardHearts_.reserve(kRewardHeartPoolCount);
    for (int index = 0; index < kRewardHeartPoolCount; ++index) {
        RewardHeart heart{};
        heart.object = std::make_unique<Object3d>();
        heart.object->Initialize(object3dCommon_.get());
        heart.object->SetModel(heartModel);
        heart.object->SetTranslate({ 0.0f, -1000.0f, 0.0f });
        heart.object->SetScale({ 0.0f, 0.0f, 1.0f });
        heart.object->SetTextureFilePath("resources/human/white.png");
        heart.object->SetColor({ 1.0f, 0.18f, 0.44f, 0.0f });
        heart.object->SetLightingMode(0);
        heart.object->SetEnvironmentCoefficient(0.0f);
        heart.object->SetAlphaReference(0.01f);
        heart.object->Update();
        rewardHearts_.push_back(std::move(heart));
    }
}

void GameRuntime::SpawnRewardHearts(const Math::Vector3& worldPosition, int count)
{
    if (count <= 0 || rewardHearts_.empty()) {
        return;
    }

    int spawnedCount = 0;
    const float seed =
        static_cast<float>(defeatedEnemyCount_ * 17 + spawnedEnemyCountInWave_ * 5) * 0.137f;
    for (RewardHeart& heart : rewardHearts_) {
        if (spawnedCount >= count) {
            break;
        }
        if (heart.isActive || !heart.object) {
            continue;
        }

        const float t =
            static_cast<float>(spawnedCount) /
            static_cast<float>((std::max)(count, 1));
        const float angle = seed + kTwoPi * t;
        const float ring = 0.085f + 0.032f * static_cast<float>(spawnedCount % 3);
        heart.position = {
            worldPosition.x + std::cos(angle) * 0.20f,
            worldPosition.y + std::sin(angle * 1.7f) * 0.14f,
            worldPosition.z + std::sin(angle) * 0.07f,
        };
        heart.velocity = {
            std::cos(angle) * ring,
            0.054f + std::sin(angle * 1.3f) * 0.030f,
            -0.052f + std::sin(angle) * 0.018f,
        };
        heart.collisionRadius = 0.46f;
        heart.age = 0.0f;
        heart.collectDelay = 8.0f + static_cast<float>(spawnedCount % 3) * 1.5f;
        heart.life = 150.0f;
        heart.phase = angle;
        heart.baseScale = 0.27f + static_cast<float>(spawnedCount % 2) * 0.035f;
        heart.scoreValue = kRewardHeartScoreValue;
        heart.isActive = true;
        ++spawnedCount;
    }

    if (spawnedCount < count) {
        rewardHeartPoolMisses_ += static_cast<size_t>(count - spawnedCount);
    }
}

void GameRuntime::UpdateRewardHearts()
{
    float frameStep = 1.0f;
    if (dxCommon_) {
        frameStep =
            std::clamp(dxCommon_->GetDeltaTime() * 60.0f, 0.5f, 3.0f);
    }

    const bool canCollect = player_ && !player_->IsDead();
    Math::Vector3 targetPosition{};
    if (canCollect) {
        targetPosition = player_->GetTranslate();
        targetPosition.y += 0.12f;
        targetPosition.z += 0.42f;
    }

    for (RewardHeart& heart : rewardHearts_) {
        if (!heart.isActive || !heart.object) {
            continue;
        }

        heart.age += frameStep;
        heart.phase += 0.12f * frameStep;

        if (!canCollect) {
            heart.position.x += heart.velocity.x * frameStep;
            heart.position.y += heart.velocity.y * frameStep;
            heart.position.z += heart.velocity.z * frameStep;
            heart.velocity = heart.velocity * 0.94f;
            if (heart.age >= heart.life) {
                heart.isActive = false;
            }
            continue;
        }

        if (heart.age < heart.collectDelay) {
            heart.position.x += heart.velocity.x * frameStep;
            heart.position.y += heart.velocity.y * frameStep;
            heart.position.z += heart.velocity.z * frameStep;
            heart.velocity = heart.velocity * std::clamp(1.0f - 0.060f * frameStep, 0.78f, 0.96f);
        } else {
            const float dx = targetPosition.x - heart.position.x;
            const float dy = targetPosition.y - heart.position.y;
            const float dz = targetPosition.z - heart.position.z;
            const float distanceSq = dx * dx + dy * dy + dz * dz;
            if (distanceSq <= heart.collisionRadius * heart.collisionRadius ||
                heart.age >= heart.life) {
                heart.isActive = false;
                score_ += heart.scoreValue;
                AddRewardHeartCollectEffect(targetPosition);
                AddCameraShake(0.004f, 2);
                continue;
            }

            const float pullRate =
                std::clamp(0.070f + (heart.age - heart.collectDelay) * 0.0038f, 0.070f, 0.36f);
            heart.position = Lerp(
                heart.position,
                targetPosition,
                std::clamp(pullRate * frameStep, 0.0f, 0.54f));
        }

        const float popRate = std::clamp(heart.age / 7.0f, 0.0f, 1.0f);
        const float pullRate =
            heart.age >= heart.collectDelay ?
            std::clamp((heart.age - heart.collectDelay) / 28.0f, 0.0f, 1.0f) :
            0.0f;
        const float pulse = 1.0f + std::sin(heart.phase * 2.6f) * 0.075f;
        const float scale = heart.baseScale * (0.55f + popRate * 0.45f) * pulse *
            (1.0f - pullRate * 0.24f);
        Math::Vector3 rotate = camera_ ? camera_->GetRotate() : Math::Vector3{};
        rotate.z += heart.phase * 0.55f;

        heart.object->SetTranslate(heart.position);
        heart.object->SetRotate(rotate);
        heart.object->SetScale({ scale, scale, 1.0f });
        heart.object->SetColor({ 1.0f, 0.18f, 0.46f, 0.95f });
        heart.object->Update();
    }
}

void GameRuntime::InitializeDepthCueEffects()
{
    depthCueEffects_.clear();
    if (!object3dCommon_) {
        return;
    }

    Model* sparkleModel = effectSparkStarModel_ ? effectSparkStarModel_ : effectGlowCoreModel_;
    Model* glowModel = effectGlowCoreModel_ ? effectGlowCoreModel_ : sparkleModel;
    Model* streakModel = effectPlayerBulletTrailModel_ ?
        effectPlayerBulletTrailModel_ :
        effectBulletTrailModel_;
    if (!sparkleModel && !glowModel && !streakModel) {
        return;
    }

    depthCueEffects_.reserve(kDepthCueEffectCount);
    for (int index = 0; index < kDepthCueEffectCount; ++index) {
        const bool isStreak = (index % 2) == 0 && streakModel;
        const bool isGold = !isStreak && (index % 7) == 0;
        const float side = PseudoRandom01(index, 0.12f) < 0.5f ? -1.0f : 1.0f;
        const float outerBias = std::pow(PseudoRandom01(index, 0.35f), 0.42f);
        const float x = side * Lerp(3.6f, 19.5f, outerBias);
        const float y = Lerp(-5.8f, 7.2f, PseudoRandom01(index, 0.61f));
        const float z = Lerp(10.0f, kDepthCueFarLocalZ, PseudoRandom01(index, 0.88f));

        DepthCueEffect cue{};
        cue.object = std::make_unique<Object3d>();
        cue.object->Initialize(object3dCommon_.get());
        cue.model = isStreak ? streakModel : ((index % 3) == 0 ? glowModel : sparkleModel);
        cue.object->SetModel(cue.model);
        cue.object->SetLightingMode(0);
        cue.object->SetEnvironmentCoefficient(0.0f);
        cue.object->SetAlphaReference(0.01f);
        cue.anchor = { x, y, z };
        cue.color = isStreak ?
            Math::Vector4{ 0.72f, 0.96f, 1.0f, 0.30f } :
            isGold ?
                Math::Vector4{ 1.0f, 0.86f, 0.42f, 0.40f } :
                Math::Vector4{ 0.78f, 0.96f, 1.0f, 0.34f };
        cue.loopLength = kDepthCueLoopLength;
        cue.speedMultiplier = isStreak ?
            Lerp(1.18f, 2.10f, PseudoRandom01(index, 1.12f)) :
            Lerp(0.86f, 1.58f, PseudoRandom01(index, 1.12f));
        cue.lateralDrift = Lerp(0.18f, 1.05f, PseudoRandom01(index, 1.46f));
        cue.verticalDrift = Lerp(0.10f, 0.62f, PseudoRandom01(index, 1.78f));
        cue.driftSpeed = Lerp(0.014f, 0.042f, PseudoRandom01(index, 2.03f));
        cue.phase = PseudoRandom01(index, 2.41f) * kTwoPi;
        cue.baseScale = isStreak ?
            Lerp(0.46f, 0.82f, PseudoRandom01(index, 2.82f)) :
            Lerp(0.09f, 0.26f, PseudoRandom01(index, 2.82f));
        cue.aspectX = isStreak ? 0.16f : 1.0f;
        cue.aspectY = isStreak ? Lerp(3.2f, 5.8f, PseudoRandom01(index, 3.17f)) : 1.0f;
        cue.spinSpeed = isStreak ?
            Lerp(-0.006f, 0.006f, PseudoRandom01(index, 3.59f)) :
            Lerp(-0.018f, 0.018f, PseudoRandom01(index, 3.59f));

        cue.object->SetColor(cue.color);
        cue.object->SetScale({
            cue.baseScale * cue.aspectX,
            cue.baseScale * cue.aspectY,
            1.0f,
        });
        cue.object->SetTranslate({
            cue.anchor.x,
            cue.anchor.y,
            railDistance_ + cue.anchor.z,
        });
        cue.object->Update();
        depthCueEffects_.push_back(std::move(cue));
    }
}

void GameRuntime::UpdateDepthCueEffects()
{
    if (depthCueEffects_.empty()) {
        return;
    }

    const Math::Vector3 cameraRotate = camera_ ? camera_->GetRotate() : Math::Vector3{};
    const float frameStep =
        dxCommon_ ? std::clamp(dxCommon_->GetDeltaTime() * 60.0f, 0.5f, 4.0f) : 1.0f;
    const float speedRate = std::clamp((railSpeed_ - 0.13f) / 0.10f, 0.0f, 1.0f);
    for (DepthCueEffect& cue : depthCueEffects_) {
        if (!cue.object) {
            continue;
        }

        float localZ = std::fmod(
            cue.anchor.z - railDistance_ * cue.speedMultiplier,
            cue.loopLength);
        while (localZ < kDepthCueNearLocalZ) {
            localZ += cue.loopLength;
        }

        const float depthRate = std::clamp(
            (localZ - kDepthCueNearLocalZ) /
                (kDepthCueFarLocalZ - kDepthCueNearLocalZ),
            0.0f,
            1.0f);
        const float motion = railDistance_ * cue.driftSpeed + cue.phase;
        const float sidePull = 1.0f + (1.0f - depthRate) * (0.28f + speedRate * 0.18f);
        Math::Vector3 position{};
        position.x = cue.anchor.x * sidePull + std::sin(motion) * cue.lateralDrift;
        position.y = cue.anchor.y + std::cos(motion * 0.77f) * cue.verticalDrift;
        position.z = railDistance_ + localZ;

        const float nearRate = 1.0f - depthRate;
        const float twinkle =
            0.78f +
            0.22f * std::sin(cue.phase + railDistance_ * (0.16f + cue.driftSpeed));
        const float scale =
            cue.baseScale *
            Lerp(0.66f, 1.68f + speedRate * 0.28f, nearRate) *
            twinkle;
        Math::Vector4 color = cue.color;
        color.w *= Lerp(0.44f, 1.22f + speedRate * 0.22f, nearRate) * twinkle;

        Math::Vector3 rotate = cameraRotate;
        rotate.z += cue.phase * 0.34f + railDistance_ * cue.spinSpeed * frameStep;

        cue.object->SetTranslate(position);
        cue.object->SetRotate(rotate);
        cue.object->SetScale({
            scale * cue.aspectX,
            scale * cue.aspectY * (1.0f + speedRate * nearRate * 0.36f),
            1.0f,
        });
        cue.object->SetColor(color);
        cue.object->Update();
    }
}

void GameRuntime::DrawDepthCueEffects()
{
    if (depthCueEffects_.empty() || !object3dCommon_) {
        return;
    }

    const BlendMode previousBlendMode = object3dCommon_->GetBlendMode();
    const DepthDrawMode previousDepthMode = object3dCommon_->GetDepthDrawMode();

    object3dCommon_->SetDepthDrawMode(DepthDrawMode::Overlay);
    object3dCommon_->SetBlendMode(BlendMode::Add);
    object3dCommon_->CommonDrawSetting();

    for (const DepthCueEffect& cue : depthCueEffects_) {
        if (cue.object) {
            cue.object->Draw();
        }
    }

    object3dCommon_->SetBlendMode(previousBlendMode);
    object3dCommon_->SetDepthDrawMode(previousDepthMode);
    object3dCommon_->CommonDrawSetting();
}

void GameRuntime::InitializeRailScenery()
{
    railSceneryObjects_.clear();

    if (!object3dCommon_) {
        return;
    }

    ModelManager* modelManager = ModelManager::GetInstance();

    const auto addSceneryStyled =
        [&](const char* modelPath,
            const Math::Vector3& anchor,
            const Math::Vector3& scale,
            const Math::Vector3& rotate,
            float loopLength,
            float phase,
            const Math::Vector4& color,
            int lightingMode,
            float environmentCoefficient) {
            modelManager->LoadModel(modelPath);
            Model* model = modelManager->FindModel(modelPath);
            if (!model) {
                return;
            }

            const std::string modelPathText = modelPath ? modelPath : "";
            RailSceneryObject scenery;
            scenery.object = std::make_unique<Object3d>();
            scenery.object->Initialize(object3dCommon_.get());
            scenery.object->SetModel(model);
            scenery.object->SetTranslate(anchor);
            scenery.object->SetRotate(rotate);
            scenery.object->SetScale(scale);
            scenery.object->SetColor(color);
            scenery.object->SetLightingMode(lightingMode);
            scenery.object->SetEnvironmentCoefficient(environmentCoefficient);
            scenery.object->SetShadowReceiveStrength(0.0f);
            if (modelPathText.find("Street") != std::string::npos) {
                scenery.object->SetShininess(24.0f);
                scenery.object->SetSpecularColor({ 0.055f, 0.060f, 0.070f });
                scenery.object->SetRoughness(0.84f);
                scenery.object->SetMetallic(0.02f);
            } else if (modelPathText.find("Building") != std::string::npos) {
                scenery.object->SetShininess(28.0f);
                scenery.object->SetSpecularColor({ 0.065f, 0.070f, 0.080f });
                scenery.object->SetRoughness(0.76f);
                scenery.object->SetMetallic(0.0f);
            } else {
                scenery.object->SetShininess(32.0f);
                scenery.object->SetSpecularColor({ 0.09f, 0.095f, 0.105f });
                scenery.object->SetRoughness(0.68f);
                scenery.object->SetMetallic(0.05f);
            }
            scenery.object->SetAlphaReference(0.01f);
            scenery.object->Update();
            scenery.anchor = anchor;
            scenery.scale = scale;
            scenery.rotate = rotate;
            scenery.color = color;
            scenery.loopLength = loopLength;
            scenery.speedMultiplier = 1.0f;
            scenery.driftSpeed = 0.0f;
            scenery.phase = phase;
            scenery.currentLocalZ = anchor.z;
            scenery.drawFarLocalZ = kSceneryFarLocalZ;
            if (modelPathText.find("Building") != std::string::npos) {
                const bool isBackRow = std::abs(anchor.x) > 40.0f;
                scenery.drawFarLocalZ = isBackRow ? 178.0f : 220.0f;
            } else if (
                modelPathText.find("Manhole") != std::string::npos ||
                modelPathText.find("Drain") != std::string::npos) {
                scenery.drawFarLocalZ = 140.0f;
            }
            scenery.isVisible = true;
            railSceneryObjects_.push_back(std::move(scenery));
        };

    const auto addScenery =
        [&](const char* modelPath,
            const Math::Vector3& anchor,
            const Math::Vector3& scale,
            const Math::Vector3& rotate,
            float loopLength,
            float phase) {
            addSceneryStyled(
                modelPath,
                anchor,
                scale,
                rotate,
                loopLength,
                phase,
                { 1.0f, 1.0f, 1.0f, 1.0f },
                2,
                0.04f);
        };

    constexpr float kCityLoopLength = 324.0f;
    constexpr float kRoadY = -3.05f;
    constexpr float kSurfaceDetailY = kRoadY - 0.135f;
    constexpr float kBuildingY = kRoadY - 0.15f;
    constexpr float kHalfPi = 1.57079632679f;

    for (int i = 0; i < 18; ++i) {
        const float segmentZ = 18.0f * static_cast<float>(i);
        addSceneryStyled(
            kCityStreet4LaneModelPath,
            { 0.0f, kRoadY, -9.0f + segmentZ },
            { 14.00f, 1.0f, 1.0f },
            { 0.0f, 0.0f, 0.0f },
            kCityLoopLength,
            0.0f,
            { 0.78f, 0.82f, 0.86f, 1.0f },
            2,
            0.015f);
    }

    struct CityBuildingPlacement {
        const char* modelPath;
        Math::Vector3 anchor;
        Math::Vector3 scale;
        Math::Vector3 rotate;
        Math::Vector4 color;
    };

    const CityBuildingPlacement cityBuildings[] = {
        { kCityBuildingLargeModelPath,
            { -31.5f, kBuildingY, 4.0f },
            { 0.95f, 1.30f, 0.95f },
            { 0.0f, kHalfPi, 0.0f },
            { 0.92f, 0.96f, 1.0f, 1.0f } },
        { kCityBuildingMediumModelPath,
            { 31.5f, kBuildingY, 10.0f },
            { 1.05f, 1.22f, 1.05f },
            { 0.0f, -kHalfPi, 0.0f },
            { 0.95f, 0.98f, 1.0f, 1.0f } },
        { kCityBuildingSmallModelPath,
            { -31.5f, kBuildingY, 24.0f },
            { 1.10f, 1.20f, 1.10f },
            { 0.0f, kHalfPi, 0.0f },
            { 0.98f, 0.97f, 0.94f, 1.0f } },
        { kCityBuildingLargeModelPath,
            { 31.5f, kBuildingY, 31.0f },
            { 1.05f, 1.42f, 1.05f },
            { 0.0f, -kHalfPi, 0.0f },
            { 0.93f, 0.96f, 1.0f, 1.0f } },
        { kCityBuildingMediumModelPath,
            { -31.5f, kBuildingY, 48.0f },
            { 1.08f, 1.28f, 1.08f },
            { 0.0f, kHalfPi, 0.0f },
            { 0.96f, 0.98f, 1.0f, 1.0f } },
        { kCityBuildingSmallModelPath,
            { 31.5f, kBuildingY, 56.0f },
            { 1.15f, 1.26f, 1.15f },
            { 0.0f, -kHalfPi, 0.0f },
            { 0.98f, 0.96f, 0.93f, 1.0f } },
        { kCityBuildingLargeModelPath,
            { -31.5f, kBuildingY, 73.0f },
            { 1.05f, 1.46f, 1.05f },
            { 0.0f, kHalfPi, 0.0f },
            { 0.92f, 0.95f, 0.99f, 1.0f } },
        { kCityBuildingMediumModelPath,
            { 31.5f, kBuildingY, 82.0f },
            { 1.10f, 1.32f, 1.10f },
            { 0.0f, -kHalfPi, 0.0f },
            { 0.96f, 0.98f, 1.0f, 1.0f } },
        { kCityBuildingSmallModelPath,
            { -31.5f, kBuildingY, 101.0f },
            { 1.12f, 1.22f, 1.12f },
            { 0.0f, kHalfPi, 0.0f },
            { 0.98f, 0.97f, 0.94f, 1.0f } },
        { kCityBuildingLargeModelPath,
            { 31.5f, kBuildingY, 111.0f },
            { 1.00f, 1.38f, 1.00f },
            { 0.0f, -kHalfPi, 0.0f },
            { 0.93f, 0.96f, 1.0f, 1.0f } },
        { kCityBuildingMediumModelPath,
            { -31.5f, kBuildingY, 132.0f },
            { 1.08f, 1.30f, 1.08f },
            { 0.0f, kHalfPi, 0.0f },
            { 0.96f, 0.98f, 1.0f, 1.0f } },
        { kCityBuildingSmallModelPath,
            { 31.5f, kBuildingY, 145.0f },
            { 1.14f, 1.24f, 1.14f },
            { 0.0f, -kHalfPi, 0.0f },
            { 0.98f, 0.96f, 0.93f, 1.0f } },
    };

    constexpr size_t kCityBuildingCount =
        sizeof(cityBuildings) / sizeof(cityBuildings[0]);
    for (size_t index = 0; index < kCityBuildingCount; ++index) {
        const CityBuildingPlacement& building = cityBuildings[index];
        addSceneryStyled(
            building.modelPath,
            building.anchor,
            building.scale,
            building.rotate,
            kCityLoopLength,
            0.0f,
            building.color,
            2,
            0.04f);
        const bool keepFarPair = index % 4 == 0 || index % 4 == 1;
        if (!keepFarPair) {
            continue;
        }
        Math::Vector3 farAnchor = building.anchor;
        farAnchor.z += kCityLoopLength * 0.5f;
        if (farAnchor.z > kSceneryFarLocalZ) {
            farAnchor.z -= kCityLoopLength;
        }
        addSceneryStyled(
            building.modelPath,
            farAnchor,
            building.scale,
            building.rotate,
            kCityLoopLength,
            0.0f,
            building.color,
            2,
            0.04f);
    }

    struct CityBackRowPlacement {
        const char* modelPath;
        float z;
        Math::Vector3 scale;
        Math::Vector4 color;
    };

    constexpr float kBackRowX = 43.5f;
    const CityBackRowPlacement cityBackRows[] = {
        { kCityBuildingLargeModelPath,  -8.0f, { 1.18f, 1.58f, 1.62f }, { 0.86f, 0.90f, 0.96f, 1.0f } },
        { kCityBuildingMediumModelPath,  34.0f, { 1.24f, 1.48f, 1.72f }, { 0.88f, 0.92f, 0.98f, 1.0f } },
        { kCityBuildingLargeModelPath,   76.0f, { 1.20f, 1.62f, 1.66f }, { 0.86f, 0.90f, 0.96f, 1.0f } },
        { kCityBuildingMediumModelPath, 118.0f, { 1.26f, 1.50f, 1.76f }, { 0.88f, 0.92f, 0.98f, 1.0f } },
        { kCityBuildingLargeModelPath,  160.0f, { 1.22f, 1.66f, 1.68f }, { 0.86f, 0.90f, 0.96f, 1.0f } },
        { kCityBuildingMediumModelPath, 202.0f, { 1.24f, 1.50f, 1.74f }, { 0.88f, 0.92f, 0.98f, 1.0f } },
        { kCityBuildingLargeModelPath,  244.0f, { 1.20f, 1.60f, 1.70f }, { 0.86f, 0.90f, 0.96f, 1.0f } },
        { kCityBuildingMediumModelPath, 286.0f, { 1.26f, 1.52f, 1.78f }, { 0.88f, 0.92f, 0.98f, 1.0f } },
    };

    constexpr size_t kCityBackRowCount =
        sizeof(cityBackRows) / sizeof(cityBackRows[0]);
    for (size_t index = 0; index < kCityBackRowCount; ++index) {
        const CityBackRowPlacement& row = cityBackRows[index];
        for (int sideIndex = 0; sideIndex < 2; ++sideIndex) {
            const float side = sideIndex == 0 ? -1.0f : 1.0f;
            const float stagger = side > 0.0f ? 21.0f : 0.0f;
            Math::Vector3 scale = row.scale;
            scale.x *= side > 0.0f ? 1.04f : 1.0f;
            scale.z *= side > 0.0f ? 1.02f : 1.0f;
            addSceneryStyled(
                row.modelPath,
                { side * kBackRowX, kBuildingY - 0.03f, row.z + stagger },
                scale,
                { 0.0f, side < 0.0f ? kHalfPi : -kHalfPi, 0.0f },
                kCityLoopLength,
                0.0f,
                row.color,
                2,
                0.03f);
        }
    }

    for (int i = 0; i < 4; ++i) {
        const float detailZ = 18.0f + 42.0f * static_cast<float>(i);
        addScenery(
            kCityManholeCoverModelPath,
            { -1.6f, kSurfaceDetailY, detailZ },
            { 1.15f, 1.15f, 1.15f },
            { 0.0f, 0.22f * static_cast<float>(i), 0.0f },
            kCityLoopLength,
            0.0f);
        addScenery(
            kCityDrainModelPath,
            { 4.2f, kSurfaceDetailY, detailZ + 8.0f },
            { 1.2f, 1.2f, 1.2f },
            { 0.0f, 0.0f, 0.0f },
            kCityLoopLength,
            0.0f);
    }
}

void GameRuntime::UpdateRailScenery()
{
    visibleSceneryCount_ = 0;
    for (RailSceneryObject& scenery : railSceneryObjects_) {
        if (!scenery.object) {
            continue;
        }

        const float localZ = WrapSceneryLocalZ(
            scenery.anchor.z -
                railDistance_ * scenery.speedMultiplier +
                scenery.phase,
            scenery.loopLength);
        scenery.currentLocalZ = localZ;
        scenery.isVisible =
            localZ >= kSceneryNearLocalZ &&
            localZ <= scenery.drawFarLocalZ;
        if (scenery.isVisible) {
            ++visibleSceneryCount_;
        }
        const float driftPhase =
            railDistance_ * scenery.driftSpeed + scenery.phase * 0.031f;
        const float railCurve =
            std::sin((railDistance_ + localZ) * 0.018f) * 1.25f;
        const float nearRate = std::clamp(
            1.0f -
                (localZ - kSceneryNearLocalZ) /
                    (kSceneryFarLocalZ - kSceneryNearLocalZ),
            0.0f,
            1.0f);

        const Math::Vector3 position{
            scenery.anchor.x +
                std::sin(driftPhase) * scenery.lateralDrift +
                railCurve * scenery.curveInfluence *
                    (0.25f + nearRate * 0.75f),
            scenery.anchor.y +
                std::cos(driftPhase * 0.82f) * scenery.verticalDrift,
            railDistance_ + localZ
        };
        const float yawWobble =
            std::abs(scenery.driftSpeed) > 0.0001f ?
            std::sin(driftPhase * 0.55f) * 0.08f :
            0.0f;
        const Math::Vector3 rotate{
            scenery.rotate.x,
            scenery.rotate.y + yawWobble,
            scenery.rotate.z + railDistance_ * scenery.rollSpeed
        };

        scenery.object->SetTranslate(position);
        scenery.object->SetRotate(rotate);
        scenery.object->Update();
    }
}

void GameRuntime::DrawRailScenery()
{
    if (railSceneryObjects_.empty() || !object3dCommon_) {
        return;
    }

    const BlendMode previousBlendMode = object3dCommon_->GetBlendMode();
    const DepthDrawMode previousDepthMode = object3dCommon_->GetDepthDrawMode();
    object3dCommon_->SetDepthDrawMode(DepthDrawMode::Normal);
    object3dCommon_->SetBlendMode(BlendMode::None);
    object3dCommon_->CommonDrawSetting();

    for (const RailSceneryObject& scenery : railSceneryObjects_) {
        if (scenery.object && scenery.isVisible) {
            scenery.object->Draw();
        }
    }

    object3dCommon_->SetBlendMode(previousBlendMode);
    object3dCommon_->SetDepthDrawMode(previousDepthMode);
    object3dCommon_->CommonDrawSetting();
}

void GameRuntime::RenderShadowMap()
{
    if (!object3dCommon_) {
        return;
    }

    const Math::Vector3 shadowFocusCenter{
        0.0f,
        10.0f,
        railDistance_ + 105.0f
    };
    if (!object3dCommon_->BeginShadowPass(shadowFocusCenter)) {
        return;
    }

    const Math::Matrix4x4& lightViewProjection =
        object3dCommon_->GetShadowLightViewProjection();

    for (const auto& sceneObject : sceneObjects_) {
        if (sceneObject) {
            sceneObject->DrawShadow(lightViewProjection);
        }
    }

    if (player_ && !player_->IsDead()) {
        player_->DrawShadow(lightViewProjection);
    }

    for (const auto& enemy : enemies_) {
        if (enemy && !enemy->IsDead()) {
            enemy->DrawShadow(lightViewProjection);
        }
    }

    object3dCommon_->EndShadowPass();
}

void GameRuntime::InitializeContactShadows()
{
    if (!object3dCommon_ || !effectContactShadowModel_) {
        return;
    }

    for (ContactShadow& shadow : contactShadows_) {
        shadow.object = std::make_unique<Object3d>();
        shadow.object->Initialize(object3dCommon_.get());
        shadow.object->SetModel(effectContactShadowModel_);
        shadow.object->SetTranslate({ 0.0f, -1000.0f, -1000.0f });
        shadow.object->SetRotate({ kContactShadowHalfPi, 0.0f, 0.0f });
        shadow.object->SetScale({ 0.01f, 0.01f, 1.0f });
        shadow.object->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
        shadow.object->SetLightingMode(0);
        shadow.object->SetEnvironmentCoefficient(0.0f);
        shadow.object->SetAlphaReference(0.01f);
        shadow.object->Update();
    }
}

void GameRuntime::DrawContactShadowObject(
    Object3d& object,
    const Math::Vector3& position,
    float scaleX,
    float scaleZ,
    float alpha,
    float yaw)
{
    const float clampedAlpha = std::clamp(alpha, 0.0f, 0.55f);
    if (clampedAlpha <= 0.001f) {
        return;
    }

    object.SetTranslate({ position.x, kContactShadowY, position.z });
    object.SetRotate({ kContactShadowHalfPi, yaw, 0.0f });
    object.SetScale({
        (std::max)(scaleX, 0.01f),
        (std::max)(scaleZ, 0.01f),
        1.0f
    });
    object.SetColor({ 0.0f, 0.0f, 0.0f, clampedAlpha });
    object.Update();
    object.Draw();
}

void GameRuntime::DrawContactShadows()
{
    if (!object3dCommon_ || !effectContactShadowModel_) {
        return;
    }

    bool hasShadowObject = false;
    for (const ContactShadow& shadow : contactShadows_) {
        if (shadow.object) {
            hasShadowObject = true;
            break;
        }
    }
    if (!hasShadowObject) {
        return;
    }

    const BlendMode previousBlendMode = object3dCommon_->GetBlendMode();
    const DepthDrawMode previousDepthMode = object3dCommon_->GetDepthDrawMode();
    object3dCommon_->SetDepthDrawMode(DepthDrawMode::ReadOnly);
    object3dCommon_->SetBlendMode(BlendMode::Normal);
    object3dCommon_->CommonDrawSetting();

    size_t shadowIndex = 0;
    const auto drawNextShadow =
        [&](const Math::Vector3& position,
            float scaleX,
            float scaleZ,
            float alpha,
            float yaw) {
            if (shadowIndex >= contactShadows_.size()) {
                return;
            }
            ContactShadow& shadow = contactShadows_[shadowIndex++];
            if (!shadow.object) {
                return;
            }
            DrawContactShadowObject(
                *shadow.object,
                position,
                scaleX,
                scaleZ,
                alpha,
                yaw);
        };

    if (player_ && !player_->IsDead()) {
        const Math::Vector3 playerPosition = player_->GetTranslate();
        const float height =
            (std::max)(playerPosition.y - kContactShadowY, 0.0f);
        const float heightFade =
            std::clamp(1.0f - height / 8.0f, 0.42f, 1.0f);
        const float dodgeStretch = player_->IsDodging() ? 0.26f : 0.0f;
        drawNextShadow(
            playerPosition,
            1.65f + height * 0.10f + dodgeStretch,
            1.00f + height * 0.055f,
            0.19f * heightFade,
            player_->IsDodging() ?
                static_cast<float>(player_->GetDodgeDirection()) * 0.20f :
                0.0f);
    }

    for (const auto& enemy : enemies_) {
        if (!enemy || enemy->IsDead()) {
            continue;
        }
        const Math::Vector3 enemyPosition = enemy->GetTranslate();
        const float localZ = enemyPosition.z - railDistance_;
        if (localZ < kSceneryNearLocalZ || localZ > 210.0f) {
            continue;
        }

        const float height =
            (std::max)(enemyPosition.y - kContactShadowY, 0.0f);
        const float heightFade =
            std::clamp(1.0f - height / 12.0f, 0.32f, 0.88f);
        const float distanceFade =
            std::clamp(1.0f - (localZ - 36.0f) / 210.0f, 0.42f, 1.0f);
        if (enemy->IsBoss()) {
            drawNextShadow(
                enemyPosition,
                6.2f + height * 0.10f,
                3.3f + height * 0.055f,
                0.20f * heightFade * distanceFade,
                std::sin(cameraTimer_ * 0.013f) * 0.20f);
        } else {
            const float radius = enemy->GetRadius();
            drawNextShadow(
                enemyPosition,
                1.25f + radius * 0.55f + height * 0.075f,
                0.78f + radius * 0.30f + height * 0.040f,
                0.15f * heightFade * distanceFade,
                std::sin(cameraTimer_ * 0.018f + enemyPosition.x) * 0.18f);
        }
    }

    constexpr float kBuildingAoLoopLength = 324.0f;
    for (int index = 0; index < 8; ++index) {
        const float localZ =
            WrapSceneryLocalZ(-4.0f + 42.0f * static_cast<float>(index) - railDistance_,
                kBuildingAoLoopLength);
        if (localZ < kSceneryNearLocalZ || localZ > 220.0f) {
            continue;
        }
        const float worldZ = railDistance_ + localZ;
        const float distanceFade =
            std::clamp(1.0f - (localZ - 28.0f) / 230.0f, 0.36f, 1.0f);
        drawNextShadow(
            { -24.2f, kContactShadowY, worldZ },
            7.5f,
            15.0f,
            0.050f * distanceFade,
            0.0f);
        drawNextShadow(
            { 24.2f, kContactShadowY, worldZ + 18.0f },
            7.8f,
            15.5f,
            0.048f * distanceFade,
            0.0f);
    }

    object3dCommon_->SetBlendMode(previousBlendMode);
    object3dCommon_->SetDepthDrawMode(previousDepthMode);
    object3dCommon_->CommonDrawSetting();
}

int GameRuntime::GetPostEffectMode() const
{
    if (isPostEffectBypassEnabled_) {
        return 0;
    }
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
    if (isBlenderLevelLoaded_) {
        editorStatusMessage_ =
            "保存できません: BlenderレベルJSONは読み込み専用です。";
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

bool GameRuntime::LoadBlenderLevelObjects(const char* path)
{
    LevelDataLoader::LevelData levelData{};
    std::string errorMessage;
    if (!LevelDataLoader::LoadFile(path, levelData, &errorMessage)) {
        editorStatusMessage_ = "Blenderレベル読み込み失敗: " + errorMessage;
        return false;
    }

    std::vector<std::unique_ptr<Object3d>> loadedObjects;
    std::vector<SceneSerializer::ObjectRecord> loadedRecords;
    size_t skippedMeshCount = 0;
    size_t unsupportedFormatCount = 0;
    size_t colliderCount = 0;

    ModelManager* modelManager = ModelManager::GetInstance();
    std::function<void(const LevelDataLoader::ObjectData&)> loadObject;
    loadObject = [&](const LevelDataLoader::ObjectData& objectData) {
        if (objectData.disabled) {
            return;
        }

        if (objectData.collider.enabled) {
            ++colliderCount;
        }

        if (objectData.type == "MESH") {
            std::string modelPath = objectData.fileName;
            constexpr const char* kResourcesPrefix = "resources/";
            if (modelPath.starts_with(kResourcesPrefix)) {
                modelPath.erase(0, std::char_traits<char>::length(kResourcesPrefix));
            }

            Model* model = nullptr;
            if (!modelPath.empty()) {
                model = modelManager->FindModel(modelPath);
                if (!model) {
                    std::string extension =
                        std::filesystem::path(modelPath).extension().string();
                    std::transform(
                        extension.begin(),
                        extension.end(),
                        extension.begin(),
                        [](unsigned char character) {
                            return static_cast<char>(std::tolower(character));
                        });
                    const bool supportedFormat =
                        extension == ".obj" ||
                        extension == ".gltf" ||
                        extension == ".glb";
                    if (supportedFormat) {
                        std::error_code error;
                        const std::filesystem::path resourcePath =
                            std::filesystem::path("resources") / modelPath;
                        if (std::filesystem::is_regular_file(resourcePath, error)) {
                            modelManager->LoadModel(modelPath);
                            model = modelManager->FindModel(modelPath);
                        }
                    } else {
                        ++unsupportedFormatCount;
                    }
                }
            }

            if (model) {
                auto sceneObject = std::make_unique<Object3d>();
                sceneObject->Initialize(object3dCommon_.get());
                sceneObject->SetModel(model);
                sceneObject->SetTranslate(objectData.translation);
                sceneObject->SetRotate(objectData.rotation);
                sceneObject->SetScale(objectData.scaling);
                sceneObject->SetEnvironmentCoefficient(0.0f);
                sceneObject->Update();

                SceneSerializer::ObjectRecord record{};
                record.name = objectData.name;
                record.translate = objectData.translation;
                record.rotate = objectData.rotation;
                record.scale = objectData.scaling;
                for (int index = 0;
                    index < static_cast<int>(
                        sizeof(kRuntimeSceneModelItems) /
                        sizeof(kRuntimeSceneModelItems[0]));
                    ++index) {
                    if (modelPath == kRuntimeSceneModelItems[index]) {
                        record.modelIndex = index;
                        break;
                    }
                }

                loadedObjects.push_back(std::move(sceneObject));
                loadedRecords.push_back(std::move(record));
            } else {
                ++skippedMeshCount;
            }
        }

        // 現在のObject3dには親Transformがないため、子も個別のObject3dとして配置する。
        // JSON上の親子構造自体はLevelDataLoaderで再帰的に保持している。
        for (const LevelDataLoader::ObjectData& child : objectData.children) {
            loadObject(child);
        }
    };

    for (const LevelDataLoader::ObjectData& objectData : levelData.objects) {
        loadObject(objectData);
    }

    if (player_ && !levelData.players.empty()) {
        const LevelDataLoader::PlayerSpawnData& playerSpawn =
            levelData.players.front();
        railDistance_ = playerSpawn.translation.z;
        player_->SetTranslate(playerSpawn.translation);
        player_->SetRotate(playerSpawn.rotation);
        player_->SetRailZ(railDistance_);
        previousPlayerTranslate_ = player_->GetTranslate();
    }

    // 通常のレール進行で生成された敵を、Blenderの配置確認用の敵へ入れ替える。
    enemies_.clear();
    for (const LevelDataLoader::EnemySpawnData& enemySpawn : levelData.enemies) {
        std::string modelPath = enemySpawn.fileName;
        constexpr const char* kResourcesPrefix = "resources/";
        if (modelPath.starts_with(kResourcesPrefix)) {
            modelPath.erase(0, std::char_traits<char>::length(kResourcesPrefix));
        }

        Model* enemyModel = nullptr;
        if (!modelPath.empty()) {
            enemyModel = modelManager->FindModel(modelPath);
            if (!enemyModel) {
                std::string extension =
                    std::filesystem::path(modelPath).extension().string();
                std::transform(
                    extension.begin(),
                    extension.end(),
                    extension.begin(),
                    [](unsigned char character) {
                        return static_cast<char>(std::tolower(character));
                    });
                const bool supportedFormat =
                    extension == ".obj" ||
                    extension == ".gltf" ||
                    extension == ".glb";
                if (supportedFormat) {
                    std::error_code error;
                    const std::filesystem::path resourcePath =
                        std::filesystem::path("resources") / modelPath;
                    if (std::filesystem::is_regular_file(resourcePath, error)) {
                        modelManager->LoadModel(modelPath);
                        enemyModel = modelManager->FindModel(modelPath);
                    }
                }
            }
        }
        if (!enemyModel) {
            enemyModel = GetEnemyModelForBehavior(Enemy::Behavior::Formation);
        }
        if (!enemyModel || !object3dCommon_) {
            continue;
        }

        auto enemy = std::make_unique<Enemy>();
        enemy->Initialize(
            object3dCommon_.get(),
            enemyModel,
            enemySpawn.translation,
            Enemy::Behavior::Formation,
            Enemy::EntryStyle::Direct);
        enemy->SetRotate(enemySpawn.rotation);
        enemies_.push_back(std::move(enemy));
    }

    sceneObjects_ = std::move(loadedObjects);
    sceneObjectRecords_ = std::move(loadedRecords);
    currentSceneFilePath_ = path ? path : "";
    isBlenderLevelLoaded_ = true;

    editorStatusMessage_ =
        "Blenderレベルを読み込みました: " + currentSceneFilePath_ +
        " / 配置 " + std::to_string(sceneObjects_.size()) +
        " / コライダー " + std::to_string(colliderCount) +
        " / PlayerSpawn " + std::to_string(levelData.players.size()) +
        " / EnemySpawn " + std::to_string(levelData.enemies.size());
    if (skippedMeshCount > 0) {
        editorStatusMessage_ +=
            " / モデル未検出 " + std::to_string(skippedMeshCount);
    }
    if (unsupportedFormatCount > 0) {
        editorStatusMessage_ +=
            " / 非対応形式 " + std::to_string(unsupportedFormatCount);
    }
    return true;
}

bool GameRuntime::LoadSceneObjects(const char* path)
{
    std::vector<SceneSerializer::ObjectRecord> records;
    SceneSerializer::SceneSettings settings{};
    if (!SceneSerializer::LoadScene(path, records, settings)) {
        return LoadBlenderLevelObjects(path);
    }
    isBlenderLevelLoaded_ = false;

    sceneObjects_.clear();
    sceneObjectRecords_.clear();

    if (settings.hasCamera && camera_) {
        cameraTranslate_ = settings.cameraTranslate;
        cameraRotate_ = settings.cameraRotate;
        cameraFovY_ = kGameplayCameraBaseFovY;
        camera_->SetTranslate(settings.cameraTranslate);
        camera_->SetRotate(settings.cameraRotate);
        camera_->SetFovY(cameraFovY_);
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
    RenderShadowMap();

    if (showSkybox_ && skybox_) {
        skybox_->Draw();
    }

    DrawRailScenery();
    DrawContactShadows();
    DrawDepthCueEffects();

    object3dCommon_->CommonDrawSetting();
    for (const auto& sceneObject : sceneObjects_) {
        sceneObject->Draw();
    }
    for (const RewardHeart& heart : rewardHearts_) {
        if (heart.isActive && heart.object) {
            heart.object->Draw();
        }
    }
    if (player_) {
        player_->Draw();
    }
    DrawPlayerFlightAura();
    DrawPlayerDodgeAfterimages();
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

void GameRuntime::PrewarmBulletPools()
{
    playerBulletPool_.clear();
    enemyBulletPool_.clear();

    if (!object3dCommon_ || !bulletModel_) {
        return;
    }

    playerBulletPool_.reserve(kTargetPlayerBulletPoolCount);
    enemyBulletPool_.reserve(kTargetEnemyBulletPoolCount);
    for (int index = 0; index < kInitialPlayerBulletPoolCount; ++index) {
        playerBulletPool_.push_back(CreatePooledPlayerBullet());
    }
    for (int index = 0; index < kInitialEnemyBulletPoolCount; ++index) {
        enemyBulletPool_.push_back(CreatePooledEnemyBullet());
    }
}

void GameRuntime::UpdateBulletPoolWarmup()
{
    if (!object3dCommon_ || !bulletModel_) {
        return;
    }
    if (isGameOver_ || isGameClear_) {
        return;
    }

    ++bulletPoolWarmupTimer_;
    if (bulletPoolWarmupTimer_ < kBulletPoolWarmupStartDelayFrames) {
        return;
    }
    if ((bulletPoolWarmupTimer_ - kBulletPoolWarmupStartDelayFrames) %
        kBulletPoolWarmupIntervalFrames != 0) {
        return;
    }
    if (input_ && input_->PushKey(DIK_SPACE)) {
        return;
    }

    if (playerBulletPool_.size() < static_cast<size_t>(kTargetPlayerBulletPoolCount)) {
        playerBulletPool_.push_back(CreatePooledPlayerBullet());
        return;
    }
    if (enemyBulletPool_.size() < static_cast<size_t>(kTargetEnemyBulletPoolCount)) {
        enemyBulletPool_.push_back(CreatePooledEnemyBullet());
    }
}

std::unique_ptr<Bullet> GameRuntime::CreatePooledPlayerBullet()
{
    const Math::Vector3 offscreenPosition{ 0.0f, -1000.0f, -1000.0f };

    auto bullet = std::make_unique<Bullet>();
    bullet->Initialize(
        object3dCommon_.get(),
        bulletModel_,
        offscreenPosition,
        { 0.0f, 0.0f, playerBulletSpeed_ },
        { 0.70f, 1.0f, 1.0f, 0.88f },
        1,
        { 0.36f, 0.36f, 0.88f },
        0.40f,
        1,
        effectBulletGlowModel_ ? effectBulletGlowModel_ : effectPlayerBulletCoreModel_,
        { 0.66f, 1.0f, 1.0f, 0.88f },
        { 0.72f, 1.75f, 1.0f },
        nullptr,
        { 0.54f, 0.96f, 1.0f, 0.64f },
        { 0.42f, 3.50f, 1.0f },
        1.78f,
        nullptr,
        { 0.70f, 1.0f, 1.0f, 0.0f },
        { 0.075f, 0.075f, 1.0f });
    bullet->Kill();
    return bullet;
}

std::unique_ptr<Bullet> GameRuntime::CreatePooledEnemyBullet()
{
    const Math::Vector3 offscreenPosition{ 0.0f, -1000.0f, -1000.0f };

    auto bullet = std::make_unique<Bullet>();
    bullet->Initialize(
        object3dCommon_.get(),
        bulletModel_,
        offscreenPosition,
        { 0.0f, 0.0f, -enemyBulletSpeed_ },
        { 1.0f, 0.18f, 0.42f, 1.0f },
        1,
        { 0.62f, 0.62f, 0.78f },
        0.66f,
        1,
        effectEnemyBulletCoreModel_ ? effectEnemyBulletCoreModel_ : effectBulletGlowModel_,
        { 1.0f, 0.18f, 0.54f, 0.95f },
        { 1.50f, 1.50f, 1.0f },
        nullptr,
        { 1.0f, 0.10f, 0.44f, 0.72f },
        { 0.62f, 4.00f, 1.0f },
        2.25f,
        nullptr,
        { 1.0f, 0.30f, 0.70f, 0.0f },
        { 0.14f, 0.14f, 1.0f });
    bullet->Kill();
    return bullet;
}

std::unique_ptr<Bullet> GameRuntime::AcquireBullet(std::vector<std::unique_ptr<Bullet>>& pool)
{
    if (pool.empty()) {
        return nullptr;
    }

    auto bullet = std::move(pool.back());
    pool.pop_back();
    return bullet;
}

void GameRuntime::PrewarmHitEffectObjectPool()
{
    hitEffectObjectPool_.clear();

    if (!object3dCommon_) {
        return;
    }

    hitEffectObjectPool_.reserve(kTargetHitEffectObjectPoolCount);
    for (int index = 0; index < kInitialHitEffectObjectPoolCount; ++index) {
        hitEffectObjectPool_.push_back(CreatePooledHitEffectObject());
    }
}

void GameRuntime::UpdateHitEffectObjectPoolWarmup()
{
    if (!object3dCommon_) {
        return;
    }
    if (isGameOver_ || isGameClear_) {
        return;
    }
    if (bulletPoolWarmupTimer_ < kHitEffectPoolWarmupStartDelayFrames) {
        return;
    }
    if ((bulletPoolWarmupTimer_ - kHitEffectPoolWarmupStartDelayFrames) %
        kHitEffectPoolWarmupIntervalFrames != 0) {
        return;
    }
    if (input_ && input_->PushKey(DIK_SPACE)) {
        return;
    }
    if (!hitEffects_.empty()) {
        return;
    }
    if (hitEffectObjectPool_.size() < static_cast<size_t>(kTargetHitEffectObjectPoolCount)) {
        hitEffectObjectPool_.push_back(CreatePooledHitEffectObject());
    }
}

std::unique_ptr<Object3d> GameRuntime::CreatePooledHitEffectObject()
{
    auto object = std::make_unique<Object3d>();
    object->Initialize(object3dCommon_.get());
    object->SetTranslate({ 0.0f, -1000.0f, -1000.0f });
    object->SetScale({ 0.01f, 0.01f, 1.0f });
    object->SetLightingMode(0);
    object->SetEnvironmentCoefficient(0.0f);
    object->Update();
    return object;
}

std::unique_ptr<Object3d> GameRuntime::AcquireHitEffectObject()
{
    if (hitEffectObjectPool_.empty()) {
        ++hitEffectObjectPoolMisses_;
        return nullptr;
    }

    auto object = std::move(hitEffectObjectPool_.back());
    hitEffectObjectPool_.pop_back();
    return object;
}

void GameRuntime::RecycleHitEffectVisuals(HitEffect& effect)
{
    for (size_t index = 0; index < effect.visualCount; ++index) {
        HitEffect::Visual& visual = effect.visuals[index];
        if (!visual.object) {
            continue;
        }
        visual.object->SetTranslate({ 0.0f, -1000.0f, -1000.0f });
        visual.object->SetScale({ 0.01f, 0.01f, 1.0f });
        visual.object->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
        visual.object->Update();
        hitEffectObjectPool_.push_back(std::move(visual.object));
    }
    effect.visualCount = 0;
}

void GameRuntime::FirePlayerBullet()
{
    if (!player_ || !bulletModel_) {
        return;
    }

    Math::Vector3 spawnPosition = player_->GetTranslate();
    spawnPosition.z += 1.75f;
    const Enemy* aimedTarget = (isReticleOnTarget_ && lockedEnemy_ && !lockedEnemy_->IsDead()) ? lockedEnemy_ : nullptr;
    Math::Vector3 aimDirection = CalculateAimDirection(spawnPosition);
    if (aimedTarget) {
        const Math::Vector3 targetPosition = aimedTarget->GetAimPosition();
        Math::Vector3 targetDirection = Math::Normalize({
            targetPosition.x - spawnPosition.x,
            targetPosition.y - spawnPosition.y,
            targetPosition.z - spawnPosition.z
        });
        if (std::abs(targetDirection.x) + std::abs(targetDirection.y) + std::abs(targetDirection.z) > 0.001f) {
            aimDirection = targetDirection;
        }
    }
    Math::Vector3 velocity = aimDirection * playerBulletSpeed_;
    Math::Vector4 color{ 0.24f, 0.72f, 1.0f, 0.92f };
    Math::Vector3 scale{ 0.44f, 0.44f, 1.04f };
    float collisionRadius = 0.42f;
    int lifeTimer = 180;
    int hitLimit = 1;
    const bool isCharged = chargeTimer_ >= chargeShotThreshold_;

    if (isCharged) {
        velocity = aimDirection * lockBulletSpeed_ * chargedBulletSpeedMultiplier_;
        color = { 0.40f, 0.84f, 1.0f, 0.98f };
        scale = { 0.74f, 0.74f, 1.70f };
        collisionRadius = 1.00f;
        lifeTimer = 260;
        hitLimit = 1;
    }

    auto bullet = AcquireBullet(playerBulletPool_);
    if (!bullet) {
        ++playerBulletPoolMisses_;
        return;
    }
    Model* coreModel =
        effectPlayerBulletCoreModel_ ? effectPlayerBulletCoreModel_ : effectBulletGlowModel_;
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
        coreModel,
        isCharged ?
            Math::Vector4{ 0.48f, 0.92f, 1.0f, 1.0f } :
            Math::Vector4{ 0.16f, 0.70f, 1.0f, 0.94f },
        isCharged ?
            Math::Vector3{ 1.55f, 3.35f, 1.0f } :
            Math::Vector3{ 0.88f, 2.05f, 1.0f },
        nullptr,
        isCharged ?
            Math::Vector4{ 0.28f, 0.78f, 1.0f, 0.88f } :
            Math::Vector4{ 0.10f, 0.46f, 1.0f, 0.66f },
        isCharged ?
            Math::Vector3{ 0.92f, 7.80f, 1.0f } :
            Math::Vector3{ 0.42f, 3.50f, 1.0f },
        isCharged ? 4.50f : 1.78f,
        isCharged ? effectSparkStarModel_ : nullptr,
        isCharged ?
            Math::Vector4{ 0.72f, 0.94f, 1.0f, 0.72f } :
            Math::Vector4{ 0.34f, 0.72f, 1.0f, 0.0f },
        isCharged ?
            Math::Vector3{ 0.17f, 0.17f, 1.0f } :
            Math::Vector3{ 0.092f, 0.092f, 1.0f });
    const Enemy* homingTarget = aimedTarget;
    if (homingTarget) {
        bullet->EnableHoming(isCharged ? 0.18f : 0.08f);
        bullet->SetHomingTarget(homingTarget->GetAimPosition());
        homingBulletTargets_[bullet.get()] = homingTarget;
    }
    playerBullets_.push_back(std::move(bullet));
    maxActivePlayerBullets_ =
        (std::max)(maxActivePlayerBullets_, playerBullets_.size());
    chargeTimer_ = 0;
    if (isCharged) {
        AddMuzzleFlashEffect(spawnPosition, true);
        chargeFlashTimer_ = 18;
    } else {
        AddMuzzleFlashEffect(spawnPosition, false);
    }
}

void GameRuntime::FireEnemyBullet(const Math::Vector3& position)
{
    if (!bulletModel_) {
        return;
    }

    Math::Vector3 spawnPosition = position;
    spawnPosition.z -= 1.0f;
    Math::Vector3 bulletDirection{ 0.0f, 0.0f, -1.0f };
    if (player_) {
        Math::Vector3 targetPosition = player_->GetTranslate();
        targetPosition.y += 0.08f;
        targetPosition.z += 0.18f;

        const Math::Vector3 toPlayer{
            targetPosition.x - spawnPosition.x,
            targetPosition.y - spawnPosition.y,
            targetPosition.z - spawnPosition.z
        };
        const float toPlayerLengthSq =
            toPlayer.x * toPlayer.x +
            toPlayer.y * toPlayer.y +
            toPlayer.z * toPlayer.z;
        if (toPlayerLengthSq > 0.0001f) {
            bulletDirection = Math::Normalize(toPlayer);
            if (bulletDirection.z > -0.10f) {
                bulletDirection.z = -0.10f;
                bulletDirection = Math::Normalize(bulletDirection);
            }
        }
    }

    auto bullet = AcquireBullet(enemyBulletPool_);
    if (!bullet) {
        ++enemyBulletPoolMisses_;
        return;
    }
    bullet->Initialize(
        object3dCommon_.get(),
        bulletModel_,
        spawnPosition,
        bulletDirection * enemyBulletSpeed_,
        { 1.0f, 0.12f, 0.46f, 1.0f },
        260,
        { 0.76f, 0.76f, 0.92f },
        0.54f,
        1,
        effectEnemyBulletCoreModel_ ? effectEnemyBulletCoreModel_ : effectBulletGlowModel_,
        { 1.0f, 0.14f, 0.58f, 0.98f },
        { 1.85f, 1.85f, 1.0f },
        nullptr,
        { 1.0f, 0.10f, 0.44f, 0.72f },
        { 0.62f, 4.00f, 1.0f },
        2.25f,
        nullptr,
        { 1.0f, 0.30f, 0.70f, 0.0f },
        { 0.14f, 0.14f, 1.0f });
    AddEnemyMuzzleFlashEffect(spawnPosition);
    enemyBullets_.push_back(std::move(bullet));
    maxActiveEnemyBullets_ =
        (std::max)(maxActiveEnemyBullets_, enemyBullets_.size());
}

void GameRuntime::SpawnEnemy()
{
    if (!object3dCommon_ || currentWaveIndex_ >= kWaveCount) {
        return;
    }

    const WaveTuning& wave = waveTuning_[currentWaveIndex_];
    const int remainingEnemyCount =
        wave.enemyCount - spawnedEnemyCountInWave_;
    if (remainingEnemyCount <= 0) {
        return;
    }

    const EnemySpawnPattern firstPattern =
        GetEnemySpawnPattern(currentWaveIndex_, spawnSequenceIndex_);
    int spawnBatchCount = 1;
    if (firstPattern.behavior == Enemy::Behavior::Formation &&
        firstPattern.entryStyle == Enemy::EntryStyle::VFormation) {
        while (spawnBatchCount < 3 && spawnBatchCount < remainingEnemyCount) {
            const EnemySpawnPattern nextPattern =
                GetEnemySpawnPattern(
                    currentWaveIndex_,
                    spawnSequenceIndex_ + spawnBatchCount);
            if (nextPattern.behavior != firstPattern.behavior ||
                nextPattern.entryStyle != firstPattern.entryStyle) {
                break;
            }
            ++spawnBatchCount;
        }
    }

    for (int index = 0; index < spawnBatchCount; ++index) {
        const EnemySpawnPattern pattern =
            GetEnemySpawnPattern(currentWaveIndex_, spawnSequenceIndex_);
        ++spawnSequenceIndex_;
        ++spawnedEnemyCountInWave_;

        Model* model = GetEnemyModelForBehavior(pattern.behavior);
        if (!model) {
            continue;
        }
        auto enemy = std::make_unique<Enemy>();
        enemy->Initialize(
            object3dCommon_.get(),
            model,
            { pattern.x, pattern.y, railDistance_ + wave.spawnLeadDistance },
            pattern.behavior,
            pattern.entryStyle,
            0,
            1.0f,
            GetEnemyTextureOverrideForBehavior(pattern.behavior));
        enemies_.push_back(std::move(enemy));
    }
}

void GameRuntime::SpawnStageEnemy(
    float x,
    float y,
    float leadDistance,
    Enemy::Behavior behavior,
    Enemy::EntryStyle entryStyle,
    int maxHpOverride,
    float scaleMultiplier)
{
    if (!object3dCommon_) {
        return;
    }

    Model* model = GetEnemyModelForBehavior(behavior);
    const char* textureOverride = GetEnemyTextureOverrideForBehavior(behavior);
    if (behavior == Enemy::Behavior::StrafeShooter &&
        maxHpOverride >= 7 &&
        enemyHeavyModel_) {
        model = enemyHeavyModel_;
        textureOverride = kEnemyHeavyTexturePath;
    }
    if (!model) {
        return;
    }
    auto enemy = std::make_unique<Enemy>();
    enemy->Initialize(
        object3dCommon_.get(),
        model,
        { x, y, railDistance_ + leadDistance },
        behavior,
        entryStyle,
        maxHpOverride,
        scaleMultiplier,
        textureOverride);
    enemies_.push_back(std::move(enemy));
    ++spawnSequenceIndex_;
    ++spawnedEnemyCountInWave_;
}

Model* GameRuntime::GetEnemyModelForBehavior(Enemy::Behavior behavior) const
{
    switch (behavior) {
    case Enemy::Behavior::DiveBomber:
        return enemySwoopModel_ ? enemySwoopModel_ : enemyModel_;
    case Enemy::Behavior::Swoop:
        return enemySwoopModel_ ? enemySwoopModel_ : enemyModel_;
    case Enemy::Behavior::StrafeShooter:
        return enemyShooterModel_ ? enemyShooterModel_ : enemyModel_;
    case Enemy::Behavior::Crossfire:
        return enemyShooterModel_ ? enemyShooterModel_ : enemyModel_;
    case Enemy::Behavior::Formation:
    default:
        return enemyFormationModel_ ? enemyFormationModel_ : enemyModel_;
    }
}

const char* GameRuntime::GetEnemyTextureOverrideForBehavior(Enemy::Behavior behavior) const
{
    switch (behavior) {
    case Enemy::Behavior::DiveBomber:
        return enemySwoopModel_ ? kEnemySwoopTexturePath : nullptr;
    case Enemy::Behavior::Swoop:
        return enemySwoopModel_ ? kEnemySwoopTexturePath : nullptr;
    case Enemy::Behavior::StrafeShooter:
        return enemyShooterModel_ ? kEnemyShooterTexturePath : nullptr;
    case Enemy::Behavior::Crossfire:
        return enemyShooterModel_ ? kEnemyShooterTexturePath : nullptr;
    case Enemy::Behavior::Formation:
    default:
        return enemyFormationModel_ ? kEnemyFormationTexturePath : nullptr;
    }
}

void GameRuntime::SpawnBossEnemy()
{
    if (bossSpawned_ ||
        bossDefeated_ ||
        isGameOver_ ||
        isGameClear_ ||
        railDistance_ < kBossSpawnDistance ||
        !object3dCommon_) {
        return;
    }

    Model* model = bossModel_ ? bossModel_ : enemyModel_;
    if (!model) {
        return;
    }

    auto enemy = std::make_unique<Enemy>();
    enemy->Initialize(
        object3dCommon_.get(),
        model,
        { 0.0f, 2.05f, railDistance_ + 98.0f },
        Enemy::Behavior::Boss,
        Enemy::EntryStyle::Direct,
        kBossMaxHp,
        1.0f,
        kBossTexturePath);
    enemies_.push_back(std::move(enemy));
    bossSpawned_ = true;
    bossIntroTimer_ = kBossIntroDuration;
    bossWarningTimer_ = (std::max)(bossWarningTimer_, 42);
    stageCombatBeatName_ = "Boss";
    AddCameraShake(0.18f, 36);
}

void GameRuntime::UpdateStageEnemyEvents()
{
    if (isGameOver_ || isGameClear_) {
        return;
    }
    if (bossSpawned_) {
        return;
    }

    const size_t eventCount =
        (std::min)(stageEnemyEventTriggered_.size(), kStageEnemyEventCount);
    int activeStageEnemyCount = 0;
    for (const auto& enemy : enemies_) {
        if (enemy && !enemy->IsDead() && !enemy->IsBoss()) {
            ++activeStageEnemyCount;
        }
    }
    int spawnedEventCountThisFrame = 0;

    for (size_t index = 0; index < eventCount; ++index) {
        const StageEnemySpawnEvent& event = kStageEnemySpawnEvents[index];
        if (stageEnemyEventTriggered_[index] ||
            railDistance_ < event.distance) {
            continue;
        }
        const bool isCrossfirePairStart =
            event.behavior == Enemy::Behavior::Crossfire &&
            (index == 0 ||
                kStageEnemySpawnEvents[index - 1].behavior != Enemy::Behavior::Crossfire ||
                kStageEnemySpawnEvents[index - 1].distance != event.distance);
        const bool isCrossfirePairEnd =
            event.behavior == Enemy::Behavior::Crossfire &&
            index > 0 &&
            kStageEnemySpawnEvents[index - 1].behavior == Enemy::Behavior::Crossfire &&
            kStageEnemySpawnEvents[index - 1].distance == event.distance &&
            stageEnemyEventTriggered_[index - 1];
        const int requiredSlots = isCrossfirePairStart ? 2 : 1;
        if ((!isCrossfirePairEnd &&
                activeStageEnemyCount + requiredSlots > kMaxActiveStageEnemiesBeforeBoss) ||
            spawnedEventCountThisFrame >= kMaxStageEnemyEventsPerFrame) {
            break;
        }

        stageEnemyEventTriggered_[index] = true;
        stageCombatBeatName_ = event.beatName;
        SpawnStageEnemy(
            event.x,
            event.y,
            event.leadDistance,
            event.behavior,
            event.entryStyle,
            event.maxHpOverride,
            event.scaleMultiplier);
        ++activeStageEnemyCount;
        ++spawnedEventCountThisFrame;
        AddCameraShake(event.shakePower, event.shakeDuration);
    }
}

void GameRuntime::UpdateEnemyWave()
{
    UpdateStageEnemyEvents();
    SpawnBossEnemy();
}

void GameRuntime::AdvanceEnemyWaveIfCleared()
{
    if (isGameOver_ || isGameClear_) {
        return;
    }

    if (railDistance_ < kStageClearDistance) {
        return;
    }

    if (!bossSpawned_ || !bossDefeated_) {
        return;
    }

    if (!enemies_.empty()) {
        return;
    }

    currentWaveIndex_ = kWaveCount - 1;
    isGameClear_ = true;
    resultTransitionTimer_ = 90;
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
    float bestScore = kLockRadiusSq;

    for (const auto& enemy : enemies_) {
        if (!enemy || enemy->IsDead() || !enemy->IsTargetable()) {
            continue;
        }

        Math::Vector2 screenPosition{};
        const Math::Vector3 enemyPosition = enemy->GetAimPosition();
        if (!TryProjectToScreen(enemyPosition, screenPosition)) {
            continue;
        }

        float projectedEnemyRadius = 0.0f;
        const float enemyRadius = enemy->GetAimRadius();
        Math::Vector2 edgeScreen{};
        if (TryProjectToScreen(
                { enemyPosition.x + enemyRadius, enemyPosition.y, enemyPosition.z },
                edgeScreen)) {
            const float dx = edgeScreen.x - screenPosition.x;
            const float dy = edgeScreen.y - screenPosition.y;
            projectedEnemyRadius =
                (std::max)(projectedEnemyRadius, std::sqrt(dx * dx + dy * dy));
        }
        if (TryProjectToScreen(
                { enemyPosition.x, enemyPosition.y + enemyRadius, enemyPosition.z },
                edgeScreen)) {
            const float dx = edgeScreen.x - screenPosition.x;
            const float dy = edgeScreen.y - screenPosition.y;
            projectedEnemyRadius =
                (std::max)(projectedEnemyRadius, std::sqrt(dx * dx + dy * dy));
        }
        const float targetHitRadius =
            std::clamp(projectedEnemyRadius * 0.82f + 10.0f, kReticleHitRadius, 72.0f);
        const float targetHitRadiusSq = targetHitRadius * targetHitRadius;

        const float dx = screenPosition.x - reticleScreen_.x;
        const float dy = screenPosition.y - reticleScreen_.y;
        const float distanceSq = dx * dx + dy * dy;
        if (distanceSq < bestScore) {
            bestScore = distanceSq;
            lockedEnemy_ = enemy.get();
            lockedEnemyScreen_ = screenPosition;
            hasLockTarget_ = true;
            isReticleOnTarget_ = distanceSq <= targetHitRadiusSq;
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
    const bool isHeavyExplosion = strength >= 1.7f;
    effect.duration = isHeavyExplosion ? 78 : 58;
    effect.strength = strength * (isHeavyExplosion ? 1.36f : 1.16f);
    effect.scoreValue = 100;
    effect.type = HitEffectType::EnemyDestroy;

    Model* fireballModel = effectExplosionFireballModel_ ? effectExplosionFireballModel_ : effectImpactBurstModel_;
    Model* smokeModel = effectExplosionSmokeModel_ ? effectExplosionSmokeModel_ : effectGlowCoreModel_;
    Model* sparksModel = effectExplosionSparksModel_ ? effectExplosionSparksModel_ : effectSparkStarModel_;
    Model* debrisModel = effectMagicShardModel_ ? effectMagicShardModel_ : effectSparkStarModel_;

    AddHitEffectVisual(effect, smokeModel, worldPosition,
        { 0.42f, 0.39f, 0.34f, 0.72f }, 1.18f, 2.15f, 0.18f, 0.06f, 1.12f, 0.92f,
        { -0.014f, 0.038f, -0.004f }, false);
    AddHitEffectVisual(effect, smokeModel, worldPosition,
        { 0.32f, 0.30f, 0.27f, 0.62f }, 1.04f, 2.42f, -0.14f, 0.12f, 0.86f, 1.05f,
        { 0.038f, 0.028f, -0.006f }, false);
    AddHitEffectVisual(effect, smokeModel, worldPosition,
        { 0.46f, 0.43f, 0.38f, 0.48f }, 0.82f, 2.70f, 0.10f, 0.20f, 1.22f, 0.78f,
        { -0.052f, 0.012f, -0.002f }, false);
    AddHitEffectVisual(effect, smokeModel, worldPosition,
        { 0.24f, 0.23f, 0.22f, 0.50f }, 0.70f, 2.32f, -0.22f, 0.24f, 0.90f, 0.82f,
        { 0.060f, -0.002f, -0.002f }, false);

    AddHitEffectVisual(effect, fireballModel, worldPosition,
        { 1.0f, 0.72f, 0.34f, 1.0f }, 1.24f, 0.88f, 0.34f, 0.00f, 1.08f, 0.92f, {});
    AddHitEffectVisual(effect, fireballModel, worldPosition,
        { 1.0f, 0.36f, 0.12f, 0.82f }, 0.92f, 1.18f, -0.44f, 0.04f, 0.88f, 1.06f,
        { 0.018f, 0.010f, 0.0f });
    AddHitEffectVisual(effect, effectGlowRingModel_ ? effectGlowRingModel_ : effectGlowCoreModel_, worldPosition,
        { 1.0f, 0.58f, 0.18f, 0.42f }, 0.92f, 2.80f, 0.0f, 0.0f, 1.14f, 0.72f, {});
    AddHitEffectVisual(effect, sparksModel, worldPosition,
        { 1.0f, 0.82f, 0.38f, 1.0f }, 0.74f, 0.86f, 0.32f, 0.00f, 1.0f, 1.0f, {});

    AddHitEffectVisual(effect, debrisModel, worldPosition,
        { 1.0f, 0.72f, 0.26f, 1.0f }, 0.40f, 0.76f, 2.40f, 0.00f, 0.38f, 1.44f,
        { -0.185f, 0.108f, 0.016f });
    AddHitEffectVisual(effect, debrisModel, worldPosition,
        { 1.0f, 0.55f, 0.18f, 0.92f }, 0.38f, 0.72f, -2.15f, 0.02f, 0.40f, 1.28f,
        { 0.198f, 0.076f, 0.014f });
    AddHitEffectVisual(effect, debrisModel, worldPosition,
        { 1.0f, 0.66f, 0.24f, 0.86f }, 0.34f, 0.68f, 1.74f, 0.04f, 0.36f, 1.18f,
        { -0.126f, -0.148f, 0.010f });
    AddHitEffectVisual(effect, debrisModel, worldPosition,
        { 1.0f, 0.84f, 0.42f, 0.82f }, 0.32f, 0.64f, -1.92f, 0.05f, 0.38f, 1.08f,
        { 0.144f, -0.132f, 0.010f });
    AddHitEffectVisual(effect, sparksModel, worldPosition,
        { 1.0f, 0.94f, 0.58f, 0.88f }, 0.38f, 0.95f, 1.15f, 0.02f, 1.0f, 1.0f,
        { -0.104f, 0.154f, 0.006f });
    AddHitEffectVisual(effect, sparksModel, worldPosition,
        { 1.0f, 0.62f, 0.20f, 0.78f }, 0.34f, 0.90f, -1.30f, 0.04f, 1.0f, 1.0f,
        { 0.118f, 0.132f, 0.006f });
    AddHitEffectVisual(effect, sparksModel, worldPosition,
        { 1.0f, 0.42f, 0.14f, 0.64f }, 0.28f, 0.82f, 0.92f, 0.08f, 1.0f, 1.0f,
        { 0.026f, -0.172f, 0.004f });
    if (effect.visualCount > 0) {
        hitEffects_.push_back(std::move(effect));
    }
}

void GameRuntime::AddEnemyImpactEffect(
    const Math::Vector3& worldPosition,
    float strength)
{
    HitEffect effect{};
    effect.worldPosition = worldPosition;
    effect.duration = 24;
    effect.strength = strength * 1.18f;
    effect.type = HitEffectType::EnemyImpact;

    Model* fireballModel = effectExplosionFireballModel_ ? effectExplosionFireballModel_ : effectImpactBurstModel_;
    Model* sparksModel = effectExplosionSparksModel_ ? effectExplosionSparksModel_ : effectSparkStarModel_;
    Model* debrisModel = effectMagicShardModel_ ? effectMagicShardModel_ : effectSparkStarModel_;

    AddHitEffectVisual(effect, fireballModel, worldPosition,
        { 1.0f, 0.54f, 0.18f, 0.88f }, 0.54f, 0.52f, 0.24f, 0.0f, 1.10f, 0.82f, {});
    AddHitEffectVisual(effect, effectGlowCoreModel_, worldPosition,
        { 1.0f, 0.74f, 0.34f, 0.54f }, 0.36f, 0.50f, -0.18f, 0.0f, 1.42f, 0.54f, {});
    AddHitEffectVisual(effect, sparksModel, worldPosition,
        { 1.0f, 0.76f, 0.32f, 0.88f }, 0.38f, 0.72f, 0.40f, 0.01f, 1.0f, 1.0f, {});
    AddHitEffectVisual(effect, debrisModel, worldPosition,
        { 1.0f, 0.88f, 0.46f, 0.95f }, 0.26f, 0.68f, -1.85f, 0.01f, 0.42f, 1.34f,
        { -0.082f, 0.052f, 0.004f });
    AddHitEffectVisual(effect, debrisModel, worldPosition,
        { 1.0f, 0.48f, 0.18f, 0.86f }, 0.24f, 0.62f, 1.95f, 0.03f, 0.42f, 1.16f,
        { 0.092f, -0.058f, 0.004f });

    if (effect.visualCount > 0) {
        hitEffects_.push_back(std::move(effect));
    }
}

void GameRuntime::AddEnemyMuzzleFlashEffect(const Math::Vector3& worldPosition)
{
    HitEffect effect{};
    effect.worldPosition = worldPosition;
    effect.duration = 14;
    effect.strength = 0.76f;
    effect.type = HitEffectType::EnemyImpact;

    AddHitEffectVisual(effect, effectGlowCoreModel_, worldPosition,
        { 1.0f, 0.06f, 0.40f, 0.62f }, 0.42f, 0.18f, 0.0f, 0.0f, 1.20f, 0.86f, {});
    AddHitEffectVisual(effect, effectSparkStarModel_, worldPosition,
        { 1.0f, 0.22f, 0.68f, 0.54f }, 0.24f, 0.42f, 1.35f, 0.02f, 0.76f, 1.24f, { 0.0f, 0.0f, -0.018f });
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

    AddHitEffectVisual(effect, effectImpactBurstModel_ ? effectImpactBurstModel_ : effectGlowCoreModel_, worldPosition,
        isCharged ?
            Math::Vector4{ 0.36f, 0.86f, 1.0f, 0.76f } :
            Math::Vector4{ 0.18f, 0.64f, 1.0f, 0.58f },
        isCharged ? 0.46f : 0.28f,
        isCharged ? 0.22f : 0.14f,
        0.0f,
        0.0f,
        1.25f,
        0.72f,
        {});
    AddHitEffectVisual(effect, effectSparkStarModel_, worldPosition,
        isCharged ?
            Math::Vector4{ 0.62f, 0.94f, 1.0f, 0.62f } :
            Math::Vector4{ 0.30f, 0.76f, 1.0f, 0.50f },
        isCharged ? 0.24f : 0.16f,
        0.36f,
        isCharged ? 1.4f : -1.0f,
        0.02f,
        1.0f,
        0.58f,
        {});

    hitEffects_.push_back(std::move(effect));
}

void GameRuntime::AddRewardHeartCollectEffect(const Math::Vector3& worldPosition)
{
    HitEffect effect{};
    effect.worldPosition = worldPosition;
    effect.duration = 16;
    effect.strength = 0.48f;
    effect.scoreValue = kRewardHeartScoreValue;
    effect.type = HitEffectType::RewardCollect;

    AddHitEffectVisual(effect, effectGlowCoreModel_, worldPosition,
        { 1.0f, 0.66f, 0.30f, 0.44f }, 0.24f, 0.08f, 0.0f, 0.0f, 1.0f, 0.82f, {});
    AddHitEffectVisual(effect, effectSparkStarModel_, worldPosition,
        { 1.0f, 0.84f, 0.46f, 0.50f }, 0.23f, 0.24f, 1.10f, 0.02f, 1.08f, 0.62f, {});
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

void GameRuntime::AddPlayerDodgeGrazeEffect(const Math::Vector3& worldPosition)
{
    HitEffect effect{};
    effect.worldPosition = worldPosition;
    effect.duration = 12;
    effect.strength = 0.58f;
    effect.type = HitEffectType::EnemyImpact;

    AddHitEffectVisual(effect, effectGlowCoreModel_, worldPosition,
        { 0.38f, 1.0f, 0.92f, 0.48f }, 0.38f, 0.12f, 0.0f, 0.0f, 1.34f, 0.42f, {});
    AddHitEffectVisual(effect, effectSparkStarModel_, worldPosition,
        { 0.84f, 1.0f, 0.96f, 0.62f }, 0.17f, 0.36f, 1.25f, 0.02f, 0.72f, 1.08f, { 0.018f, 0.012f, 0.0f });

    if (effect.visualCount > 0) {
        hitEffects_.push_back(std::move(effect));
    }
}

void GameRuntime::TriggerJustDodge(Bullet& bullet, const Math::Vector3& worldPosition)
{
    if (!player_ || player_->IsDead()) {
        return;
    }
    if (!justDodgedEnemyBullets_.insert(&bullet).second) {
        return;
    }

    score_ += kJustDodgeScoreBonus;
    chargeTimer_ = (std::min)(chargeTimer_ + kJustDodgeChargeBonus, kChargeShotMax);
    chargeFlashTimer_ = (std::max)(chargeFlashTimer_, 26);
    justDodgeFlashTimer_ = (std::max)(justDodgeFlashTimer_, kJustDodgeFlashDuration);
    justDodgeSlowTimer_ = (std::max)(justDodgeSlowTimer_, kJustDodgeSlowDuration);

    const Math::Vector3 playerPosition = player_->GetTranslate();
    AddPlayerDodgeGrazeEffect(worldPosition);
    AddPlayerDodgeGrazeEffect({
        playerPosition.x,
        playerPosition.y + 0.12f,
        playerPosition.z + 0.10f
    });
    SpawnPlayerDodgeAfterimage();
    playerDodgeAfterimageTimer_ = 0;
    AddCameraShake(0.165f, 18);
}

void GameRuntime::TriggerPlayerImpactMoment(
    bool isCharged,
    bool isBossHit,
    bool isDestroyed)
{
    if (!isCharged && !(isBossHit && isDestroyed)) {
        return;
    }

    const int flashDuration =
        isBossHit ?
            (isDestroyed ? 28 : 15) :
            (isDestroyed ? 13 : kPlayerImpactFlashDuration);
    const int slowDuration =
        isBossHit ?
            (isDestroyed ? 24 : 11) :
            (isDestroyed ? 10 : kPlayerImpactSlowDuration);
    const float slowScale =
        isBossHit ?
            (isDestroyed ? 0.18f : 0.32f) :
            kPlayerImpactRailSlowScale;

    if (flashDuration >= playerImpactFlashTimer_) {
        playerImpactFlashDuration_ = flashDuration;
    }
    playerImpactFlashTimer_ =
        (std::max)(playerImpactFlashTimer_, flashDuration);

    if (slowDuration >= playerImpactSlowTimer_) {
        playerImpactSlowDuration_ = slowDuration;
    }
    playerImpactSlowTimer_ =
        (std::max)(playerImpactSlowTimer_, slowDuration);
    playerImpactSlowScale_ =
        playerImpactSlowTimer_ > 0 ?
        (std::min)(playerImpactSlowScale_, slowScale) :
        slowScale;

    AddCameraShake(
        isBossHit ? (isDestroyed ? 0.32f : 0.12f) : 0.070f,
        isBossHit ? (isDestroyed ? 42 : 14) : 9);
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
    const Math::Vector3& velocity,
    bool additive)
{
    if (!object3dCommon_ || !model) {
        return;
    }
    if (effect.visualCount >= HitEffect::kMaxVisuals) {
        return;
    }

    HitEffect::Visual& visual = effect.visuals[effect.visualCount];
    visual = HitEffect::Visual{};
    visual.object = AcquireHitEffectObject();
    if (!visual.object) {
        return;
    }
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
    visual.additive = additive;
    ++effect.visualCount;
}

void GameRuntime::UpdateHitEffects()
{
    float effectFrameStep = 1.0f;
    if (dxCommon_) {
        effectFrameStep =
            std::clamp(dxCommon_->GetDeltaTime() * 60.0f, 0.5f, 4.0f);
    }

    for (auto iterator = hitEffects_.begin(); iterator != hitEffects_.end();) {
        iterator->age += effectFrameStep;
        if (iterator->age >= static_cast<float>(iterator->duration)) {
            RecycleHitEffectVisuals(*iterator);
            iterator = hitEffects_.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

void GameRuntime::DrawHitEffects()
{
    return;
}

void GameRuntime::InitializePlayerDodgeAfterimages()
{
    playerDodgeAfterimageTimer_ = 0;
    nextPlayerDodgeAfterimageIndex_ = 0;
    wasPlayerDodging_ = false;
    Model* afterimageModel =
        effectPlayerBulletTrailModel_ ? effectPlayerBulletTrailModel_ : effectGlowCoreModel_;

    for (PlayerDodgeAfterimage& afterimage : playerDodgeAfterimages_) {
        afterimage = PlayerDodgeAfterimage{};
        afterimage.object = std::make_unique<Object3d>();
        afterimage.object->Initialize(object3dCommon_.get());
        if (afterimageModel) {
            afterimage.object->SetModel(afterimageModel);
        }
        afterimage.object->SetTranslate({ 0.0f, -1000.0f, -1000.0f });
        afterimage.object->SetScale({ 0.01f, 0.01f, 1.0f });
        afterimage.object->SetColor({ 0.40f, 0.96f, 1.0f, 0.0f });
        afterimage.object->SetLightingMode(0);
        afterimage.object->SetEnvironmentCoefficient(0.0f);
        afterimage.object->Update();
    }
}

void GameRuntime::InitializePlayerFlightAura()
{
    Model* softGlowModel = effectGlowCoreModel_;
    Model* exhaustTrailModel =
        effectPlayerBulletTrailModel_ ? effectPlayerBulletTrailModel_ : softGlowModel;
    Model* exhaustOuterModel =
        effectPlayerChargeTrailModel_ ? effectPlayerChargeTrailModel_ : exhaustTrailModel;
    Model* exhaustCoreModel =
        effectPlayerBulletCoreModel_ ? effectPlayerBulletCoreModel_ : softGlowModel;
    if (!object3dCommon_ || !softGlowModel) {
        return;
    }

    auto setupAura =
        [this](
            size_t index,
            Model* model,
            const Math::Vector3& offset,
            const Math::Vector4& color,
            float baseSize,
            float aspectX,
            float aspectY,
            float pulseOffset,
            float roll,
            float rollSpeed) {
            if (index >= playerFlightAuras_.size() || !model) {
                return;
            }

            PlayerFlightAura& aura = playerFlightAuras_[index];
            aura = PlayerFlightAura{};
            aura.object = std::make_unique<Object3d>();
            aura.object->Initialize(object3dCommon_.get());
            aura.object->SetModel(model);
            aura.object->SetLightingMode(0);
            aura.object->SetEnvironmentCoefficient(0.0f);
            aura.object->SetAlphaReference(0.01f);
            aura.object->SetTranslate({ 0.0f, -1000.0f, -1000.0f });
            aura.object->SetScale({ 0.01f, 0.01f, 1.0f });
            aura.object->SetColor({ color.x, color.y, color.z, 0.0f });
            aura.object->Update();
            aura.model = model;
            aura.offset = offset;
            aura.color = color;
            aura.baseSize = baseSize;
            aura.aspectX = aspectX;
            aura.aspectY = aspectY;
            aura.pulseOffset = pulseOffset;
            aura.roll = roll;
            aura.rollSpeed = rollSpeed;
        };

    setupAura(
        0,
        exhaustOuterModel,
        { -0.72f, -0.11f, -1.35f },
        { 0.10f, 0.55f, 1.0f, 0.68f },
        0.58f,
        1.05f,
        1.12f,
        0.0f,
        0.0f,
        0.052f);
    setupAura(
        1,
        exhaustOuterModel,
        { 0.72f, -0.11f, -1.35f },
        { 0.10f, 0.55f, 1.0f, 0.68f },
        0.58f,
        1.05f,
        1.12f,
        1.8f,
        0.0f,
        0.061f);
    setupAura(
        2,
        exhaustTrailModel,
        { -0.72f, -0.10f, -1.39f },
        { 0.80f, 0.97f, 1.0f, 0.95f },
        0.40f,
        0.82f,
        1.02f,
        3.6f,
        0.0f,
        0.074f);
    setupAura(
        3,
        exhaustTrailModel,
        { 0.72f, -0.10f, -1.39f },
        { 0.80f, 0.97f, 1.0f, 0.95f },
        0.40f,
        0.82f,
        1.02f,
        5.4f,
        0.0f,
        0.079f);
    setupAura(
        4,
        exhaustCoreModel,
        { -0.72f, -0.11f, -1.32f },
        { 1.0f, 1.0f, 1.0f, 0.96f },
        0.36f,
        0.90f,
        0.90f,
        2.7f,
        0.0f,
        0.068f);
    setupAura(
        5,
        exhaustCoreModel,
        { 0.72f, -0.11f, -1.32f },
        { 1.0f, 1.0f, 1.0f, 0.96f },
        0.36f,
        0.90f,
        0.90f,
        4.5f,
        0.0f,
        0.083f);
}

void GameRuntime::InitializePlayerExhaustParticles()
{
    Model* particleModel =
        effectGlowCoreModel_ ? effectGlowCoreModel_ : effectPlayerBulletCoreModel_;
    if (!object3dCommon_ || !particleModel) {
        return;
    }

    for (PlayerExhaustParticle& particle : playerExhaustParticles_) {
        particle = PlayerExhaustParticle{};
        particle.object = std::make_unique<Object3d>();
        particle.object->Initialize(object3dCommon_.get());
        particle.object->SetModel(particleModel);
        particle.object->SetLightingMode(0);
        particle.object->SetEnvironmentCoefficient(0.0f);
        particle.object->SetAlphaReference(0.01f);
        particle.object->SetTranslate({ 0.0f, -1000.0f, -1000.0f });
        particle.object->SetScale({ 0.01f, 0.01f, 1.0f });
        particle.object->SetColor({ 0.35f, 0.82f, 1.0f, 0.0f });
        particle.object->Update();
        particle.model = particleModel;
    }

    nextPlayerExhaustParticleIndex_ = 0;
}

void GameRuntime::EmitPlayerExhaustParticles(
    const Math::Vector3& playerPosition,
    const Math::Vector3& playerRotate)
{
    if (!dxCommon_) {
        return;
    }

    playerExhaustParticleTimer_ += dxCommon_->GetDeltaTime();
    if (playerExhaustParticleTimer_ < kPlayerExhaustParticleInterval) {
        return;
    }
    playerExhaustParticleTimer_ = std::fmod(
        playerExhaustParticleTimer_,
        kPlayerExhaustParticleInterval);

    const Math::Vector3 leftOffset =
        RotateLocalOffset({ -0.72f, -0.10f, -1.38f }, playerRotate);
    const Math::Vector3 rightOffset =
        RotateLocalOffset({ 0.72f, -0.10f, -1.38f }, playerRotate);
    const Math::Vector3 exhaustDirection = Math::Normalize(
        RotateLocalOffset({ 0.0f, -0.06f, -1.0f }, playerRotate));
    const Math::Vector3 leftPosition{
        playerPosition.x + leftOffset.x,
        playerPosition.y + leftOffset.y,
        playerPosition.z + leftOffset.z
    };
    const Math::Vector3 rightPosition{
        playerPosition.x + rightOffset.x,
        playerPosition.y + rightOffset.y,
        playerPosition.z + rightOffset.z
    };
    const auto spawnParticle =
        [this, &playerRotate, &exhaustDirection](
            const Math::Vector3& nozzlePosition,
            bool isCore,
            float phase) {
            PlayerExhaustParticle& particle =
                playerExhaustParticles_[nextPlayerExhaustParticleIndex_];
            nextPlayerExhaustParticleIndex_ =
                (nextPlayerExhaustParticleIndex_ + 1) % playerExhaustParticles_.size();

            if (!particle.object) {
                return;
            }

            Model* model = isCore ?
                (effectPlayerBulletCoreModel_ ?
                    effectPlayerBulletCoreModel_ : effectGlowCoreModel_) :
                (effectGlowCoreModel_ ?
                    effectGlowCoreModel_ : effectPlayerBulletCoreModel_);
            if (!model) {
                return;
            }

            const float wobbleX = std::sin(phase * 1.73f) * 0.10f;
            const float wobbleY = std::cos(phase * 2.11f) * 0.07f;
            const Math::Vector3 positionJitter = RotateLocalOffset(
                { wobbleX, wobbleY, -0.03f },
                playerRotate);
            const Math::Vector3 velocityJitter = RotateLocalOffset(
                { wobbleX * 2.6f, wobbleY * 2.0f, 0.0f },
                playerRotate);
            const float speed =
                (isCore ? 6.6f : 4.8f) + playerExhaustThrust_ * 2.1f;

            particle.model = model;
            particle.position = {
                nozzlePosition.x + positionJitter.x,
                nozzlePosition.y + positionJitter.y,
                nozzlePosition.z + positionJitter.z
            };
            particle.velocity = {
                exhaustDirection.x * speed + velocityJitter.x,
                exhaustDirection.y * speed + velocityJitter.y,
                exhaustDirection.z * speed + velocityJitter.z
            };
            particle.color = isCore ?
                Math::Vector4{ 0.78f, 0.97f, 1.0f, 0.92f } :
                Math::Vector4{ 0.06f, 0.48f, 1.0f, 0.66f };
            particle.age = 0.0f;
            particle.lifetime = isCore ? 0.15f : 0.28f;
            particle.startSize =
                (isCore ? 0.16f : 0.27f) * (0.92f + playerExhaustThrust_ * 0.22f);
            particle.endSize = isCore ? 0.025f : 0.075f;
            particle.aspectX = isCore ? 0.74f : 1.08f;
            particle.aspectY = isCore ? 1.24f : 1.16f;
            particle.roll = phase * 0.37f;
            particle.rollSpeed = (isCore ? 2.8f : 1.6f) *
                (std::sin(phase) >= 0.0f ? 1.0f : -1.0f);
            particle.isActive = true;
        };

    const float phase = cameraTimer_ * 0.173f +
        static_cast<float>(nextPlayerExhaustParticleIndex_) * 0.61f;
    spawnParticle(leftPosition, false, phase);
    spawnParticle(rightPosition, false, phase + 1.7f);
    spawnParticle(leftPosition, true, phase + 3.1f);
    spawnParticle(rightPosition, true, phase + 4.8f);
}

void GameRuntime::UpdateAndDrawPlayerExhaustParticles()
{
    if (!camera_ || !dxCommon_) {
        return;
    }

    const float deltaTime = std::clamp(dxCommon_->GetDeltaTime(), 0.0f, 1.0f / 20.0f);
    const Math::Vector3 cameraRotate = camera_->GetRotate();
    for (PlayerExhaustParticle& particle : playerExhaustParticles_) {
        if (!particle.isActive || !particle.object || !particle.model) {
            continue;
        }

        particle.age += deltaTime;
        if (particle.age >= particle.lifetime) {
            particle.isActive = false;
            particle.object->SetTranslate({ 0.0f, -1000.0f, -1000.0f });
            particle.object->SetScale({ 0.01f, 0.01f, 1.0f });
            particle.object->SetColor({ particle.color.x, particle.color.y, particle.color.z, 0.0f });
            particle.object->Update();
            continue;
        }

        const float lifeRate = std::clamp(
            particle.age / (std::max)(particle.lifetime, 0.001f),
            0.0f,
            1.0f);
        particle.position.x += particle.velocity.x * deltaTime;
        particle.position.y += particle.velocity.y * deltaTime;
        particle.position.z += particle.velocity.z * deltaTime;
        const float damping = std::pow(0.94f, deltaTime * 60.0f);
        particle.velocity.x *= damping;
        particle.velocity.y *= damping;
        particle.velocity.z *= damping;
        particle.roll += particle.rollSpeed * deltaTime;

        const float fade = (1.0f - lifeRate) * (1.0f - lifeRate);
        const float size =
            particle.startSize + (particle.endSize - particle.startSize) * lifeRate;
        const float pulse = 1.0f +
            std::sin(cameraTimer_ * 0.29f + particle.roll * 2.0f) * 0.06f;
        Math::Vector4 color = particle.color;
        color.w *= fade;
        Math::Vector3 rotate = cameraRotate;
        rotate.z += particle.roll;

        particle.object->SetModel(particle.model);
        particle.object->SetTranslate(particle.position);
        particle.object->SetRotate(rotate);
        particle.object->SetScale({
            size * particle.aspectX * pulse,
            size * particle.aspectY * (2.0f - pulse),
            1.0f
        });
        particle.object->SetColor(color);
        particle.object->Update();
        particle.object->Draw();
    }
}

void GameRuntime::SpawnPlayerDodgeAfterimage()
{
    Model* afterimageModel =
        effectPlayerBulletTrailModel_ ? effectPlayerBulletTrailModel_ : effectGlowCoreModel_;
    if (!player_ || player_->IsDead() || !afterimageModel) {
        return;
    }

    PlayerDodgeAfterimage& afterimage =
        playerDodgeAfterimages_[nextPlayerDodgeAfterimageIndex_];
    if (!afterimage.object) {
        if (!object3dCommon_) {
            return;
        }
        afterimage.object = std::make_unique<Object3d>();
        afterimage.object->Initialize(object3dCommon_.get());
    }

    afterimage.object->SetModel(afterimageModel);
    afterimage.position = player_->GetTranslate();
    afterimage.position.y += 0.06f;
    afterimage.position.z += 0.06f;
    afterimage.direction = player_->GetDodgeDirection() >= 0 ? 1 : -1;
    afterimage.age = 0.0f;
    afterimage.duration = kPlayerDodgeAfterimageDuration;
    afterimage.isActive = true;

    nextPlayerDodgeAfterimageIndex_ =
        (nextPlayerDodgeAfterimageIndex_ + 1) % playerDodgeAfterimages_.size();
}

void GameRuntime::UpdatePlayerDodgeAfterimages()
{
    float effectFrameStep = 1.0f;
    if (dxCommon_) {
        effectFrameStep =
            std::clamp(dxCommon_->GetDeltaTime() * 60.0f, 0.5f, 4.0f);
    }

    const bool isDodging =
        player_ && !player_->IsDead() && player_->IsDodging();
    const bool isJustDodgeCinematic =
        player_ && !player_->IsDead() && justDodgeSlowTimer_ > 0;
    if (playerDodgeAfterimageTimer_ > 0) {
        --playerDodgeAfterimageTimer_;
    }
    if (isDodging && !wasPlayerDodging_ && player_) {
        const Math::Vector3 playerPosition = player_->GetTranslate();
        const float direction = static_cast<float>(player_->GetDodgeDirection() >= 0 ? 1 : -1);
        AddPlayerDodgeGrazeEffect({
            playerPosition.x - direction * 0.58f,
            playerPosition.y + 0.10f,
            playerPosition.z + 0.16f
        });
        AddCameraShake(0.060f, 10);
    }
    if ((isDodging && (!wasPlayerDodging_ || playerDodgeAfterimageTimer_ <= 0)) ||
        (isJustDodgeCinematic && playerDodgeAfterimageTimer_ <= 0)) {
        SpawnPlayerDodgeAfterimage();
        playerDodgeAfterimageTimer_ =
            isJustDodgeCinematic ? 1 : kPlayerDodgeAfterimageIntervalFrames;
    }
    wasPlayerDodging_ = isDodging;

    for (PlayerDodgeAfterimage& afterimage : playerDodgeAfterimages_) {
        if (!afterimage.isActive) {
            continue;
        }

        afterimage.age += effectFrameStep;
        if (afterimage.age >= afterimage.duration) {
            afterimage.isActive = false;
            if (afterimage.object) {
                afterimage.object->SetTranslate({ 0.0f, -1000.0f, -1000.0f });
                afterimage.object->SetScale({ 0.01f, 0.01f, 1.0f });
                afterimage.object->SetColor({ 0.45f, 1.0f, 0.92f, 0.0f });
                afterimage.object->Update();
            }
        }
    }
}

void GameRuntime::DrawPlayerFlightAura()
{
    if (!object3dCommon_ || !camera_ || !player_ || player_->IsDead()) {
        return;
    }

    bool hasAuraObject = false;
    for (const PlayerFlightAura& aura : playerFlightAuras_) {
        if (aura.object && aura.model) {
            hasAuraObject = true;
            break;
        }
    }
    if (!hasAuraObject) {
        return;
    }

    const BlendMode previousBlendMode = object3dCommon_->GetBlendMode();
    const DepthDrawMode previousDepthMode = object3dCommon_->GetDepthDrawMode();

    object3dCommon_->SetDepthDrawMode(DepthDrawMode::ReadOnly);
    object3dCommon_->SetBlendMode(BlendMode::Add);
    object3dCommon_->CommonDrawSetting();

    const Math::Vector3 playerPosition = player_->GetTranslate();
    const Math::Vector3 playerRotate = player_->GetVisualRotate();
    const Math::Vector3 cameraRotate = camera_->GetRotate();
    const float speedRate = std::clamp((railSpeed_ - 0.13f) / 0.10f, 0.0f, 1.0f);
    const float accelerationRate = std::clamp(
        (targetRailSpeed_ - railSpeed_) / 0.045f,
        0.0f,
        1.0f);
    const float dodgeBoost = player_->IsDodging() ? 1.0f : 0.0f;
    const float justDodgeBoost =
        justDodgeFlashTimer_ > 0 ?
        static_cast<float>(justDodgeFlashTimer_) /
            static_cast<float>((std::max)(kJustDodgeFlashDuration, 1)) :
        0.0f;
    const float targetThrust = std::clamp(
        0.10f +
            speedRate * 0.42f +
            accelerationRate * 0.20f +
            dodgeBoost * 0.82f +
            justDodgeBoost * 0.34f,
        0.0f,
        1.55f);
    float effectFrameStep = 1.0f;
    if (dxCommon_) {
        effectFrameStep = std::clamp(dxCommon_->GetDeltaTime() * 60.0f, 0.25f, 3.0f);
    }
    const float thrustResponse =
        targetThrust > playerExhaustThrust_ ?
        kPlayerExhaustRiseResponse :
        kPlayerExhaustFallResponse;
    const float thrustBlend =
        1.0f - std::pow(1.0f - thrustResponse, effectFrameStep);
    playerExhaustThrust_ +=
        (targetThrust - playerExhaustThrust_) * thrustBlend;
    EmitPlayerExhaustParticles(playerPosition, playerRotate);
    const float exhaustAxisWobble =
        std::sin(cameraTimer_ * 0.071f) * 0.012f;

    for (size_t auraIndex = 0; auraIndex < playerFlightAuras_.size(); ++auraIndex) {
        PlayerFlightAura& aura = playerFlightAuras_[auraIndex];
        if (!aura.object || !aura.model) {
            continue;
        }

        const float pulse =
            1.0f + std::sin(cameraTimer_ * 0.074f + aura.pulseOffset) * 0.055f;
        const float flicker = std::clamp(
            0.90f +
                std::sin(cameraTimer_ * 0.137f + aura.pulseOffset) * 0.075f +
                std::sin(cameraTimer_ * 0.311f + aura.pulseOffset * 1.7f) * 0.035f,
            0.78f,
            1.08f);
        const bool isOuterFlame = auraIndex < 2;
        const bool isInnerFlame = auraIndex >= 2 && auraIndex < 4;
        const float lengthResponse =
            isOuterFlame ? 0.92f :
            isInnerFlame ? 0.66f :
            0.22f;
        const float widthResponse =
            isOuterFlame ? 0.18f :
            isInnerFlame ? 0.12f :
            0.20f;
        const float alphaResponse =
            isOuterFlame ? 0.26f :
            isInnerFlame ? 0.20f :
            0.14f;
        Math::Vector3 localOffset = aura.offset;
        localOffset.z -=
            playerExhaustThrust_ * aura.baseSize * lengthResponse * 0.05f;
        const Math::Vector3 rotatedOffset = RotateLocalOffset(localOffset, playerRotate);
        const Math::Vector3 translate{
            playerPosition.x + rotatedOffset.x,
            playerPosition.y + rotatedOffset.y,
            playerPosition.z + rotatedOffset.z
        };
        Math::Vector3 rotate = cameraRotate;
        rotate.z += playerRotate.z + aura.roll + exhaustAxisWobble;
        Math::Vector4 color = aura.color;
        color.w *= flicker *
            (0.76f + speedRate * 0.10f + playerExhaustThrust_ * alphaResponse);
        color.w = std::clamp(color.w, 0.0f, 1.0f);

        aura.object->SetModel(aura.model);
        aura.object->SetTranslate(translate);
        aura.object->SetRotate(rotate);
        aura.object->SetScale({
            aura.baseSize * aura.aspectX * pulse *
                (1.0f + playerExhaustThrust_ * widthResponse),
            aura.baseSize * aura.aspectY * (2.0f - pulse) *
                (1.0f + playerExhaustThrust_ * lengthResponse),
            1.0f
        });
        aura.object->SetColor(color);
        aura.object->Update();
        aura.object->Draw();
    }

    UpdateAndDrawPlayerExhaustParticles();

    object3dCommon_->SetBlendMode(previousBlendMode);
    object3dCommon_->SetDepthDrawMode(previousDepthMode);
    object3dCommon_->CommonDrawSetting();
}

void GameRuntime::DrawPlayerDodgeAfterimages()
{
    Model* afterimageModel =
        effectPlayerBulletTrailModel_ ? effectPlayerBulletTrailModel_ : effectGlowCoreModel_;
    if (!object3dCommon_ || !camera_ || !afterimageModel) {
        return;
    }

    bool hasActiveAfterimage = false;
    for (const PlayerDodgeAfterimage& afterimage : playerDodgeAfterimages_) {
        if (afterimage.isActive && afterimage.object) {
            hasActiveAfterimage = true;
            break;
        }
    }
    if (!hasActiveAfterimage) {
        return;
    }

    const BlendMode previousBlendMode = object3dCommon_->GetBlendMode();
    const DepthDrawMode previousDepthMode = object3dCommon_->GetDepthDrawMode();

    object3dCommon_->SetDepthDrawMode(DepthDrawMode::Overlay);
    object3dCommon_->SetBlendMode(BlendMode::Add);
    object3dCommon_->CommonDrawSetting();

    for (PlayerDodgeAfterimage& afterimage : playerDodgeAfterimages_) {
        if (!afterimage.isActive || !afterimage.object) {
            continue;
        }

        const float rate = std::clamp(
            afterimage.age / (std::max)(afterimage.duration, 1.0f),
            0.0f,
            1.0f);
        const float fade = 1.0f - rate;
        const float direction = static_cast<float>(afterimage.direction);
        Math::Vector3 translate = afterimage.position;
        translate.x -= direction * (0.24f + 1.55f * rate);
        translate.y += std::sin(rate * kTwoPi) * 0.045f;
        translate.z -= 0.28f * rate;

        Math::Vector3 rotate = camera_->GetRotate();
        rotate.z += direction * (0.26f + 0.18f * rate);

        const float length = 2.75f + 2.25f * rate;
        const float thickness = 0.56f + 0.18f * rate;
        Math::Vector4 color{
            0.46f,
            0.96f,
            1.0f,
            0.74f * fade * fade
        };

        afterimage.object->SetModel(afterimageModel);
        afterimage.object->SetTranslate(translate);
        afterimage.object->SetScale({ length, thickness, 1.0f });
        afterimage.object->SetRotate(rotate);
        afterimage.object->SetColor(color);
        afterimage.object->Update();
        afterimage.object->Draw();
    }

    object3dCommon_->SetBlendMode(previousBlendMode);
    object3dCommon_->SetDepthDrawMode(previousDepthMode);
    object3dCommon_->CommonDrawSetting();
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

    auto drawVisualPass = [&](bool additivePass) {
        object3dCommon_->SetBlendMode(additivePass ? BlendMode::Add : BlendMode::Normal);
        object3dCommon_->CommonDrawSetting();

        for (const HitEffect& effect : hitEffects_) {
            if (effect.visualCount == 0) {
                continue;
            }

            const float rate =
                effect.age /
                static_cast<float>((std::max)(effect.duration, 1));
            for (size_t index = 0; index < effect.visualCount; ++index) {
                const HitEffect::Visual& visual = effect.visuals[index];
                if (!visual.object || visual.additive != additivePass) {
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
                    effect.type == HitEffectType::RewardCollect ? fade :
                    effect.type == HitEffectType::PlayerDamage ? fade * fade * fade :
                    effect.type == HitEffectType::EnemyImpact ? fade :
                    fade * (0.78f + 0.22f * fade);
                color.w *= alphaCurve;

                Math::Vector3 rotate = camera_ ? camera_->GetRotate() : Math::Vector3{};
                rotate.z += localRate * visual.spin;
                Math::Vector3 translate = effect.worldPosition;
                translate.x += visual.velocity.x * effect.age;
                translate.y += visual.velocity.y * effect.age;
                translate.z += visual.velocity.z * effect.age;

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
    };

    drawVisualPass(false);
    drawVisualPass(true);

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
        size_t activeRewardHeartCount = 0;
        for (const RewardHeart& heart : rewardHearts_) {
            if (heart.isActive && heart.object) {
                ++activeRewardHeartCount;
            }
        }
        ImGui::Selectable((std::string(ICON_FA_HEART " reward hearts x ") + std::to_string(activeRewardHeartCount)).c_str(), false);
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
        static float frameMsHistory[120]{};
        static int historyOffset = 0;
        const ImGuiIO& io = ImGui::GetIO();
        const float frameMs = io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f;
        size_t activeRewardHeartCount = 0;
        for (const RewardHeart& heart : rewardHearts_) {
            if (heart.isActive && heart.object) {
                ++activeRewardHeartCount;
            }
        }
        size_t hitVisualCount = 0;
        for (const HitEffect& effect : hitEffects_) {
            hitVisualCount += effect.visualCount;
        }
        const size_t activePlayerBulletCount = playerBullets_.size();
        const size_t activeEnemyBulletCount = enemyBullets_.size();
        const size_t activeBulletCount = activePlayerBulletCount + activeEnemyBulletCount;
        const size_t activeBulletDrawEstimate =
            activePlayerBulletCount * 3u +
            activeEnemyBulletCount * 2u;
        const size_t pooledBulletCount = playerBulletPool_.size() + enemyBulletPool_.size();
        const size_t activeSceneryCount = visibleSceneryCount_;
        const size_t totalSceneryCount = railSceneryObjects_.size();
        const size_t activeDepthCueCount = depthCueEffects_.size();
        const size_t estimatedActiveDrawObjects =
            sceneObjects_.size() +
            activeRewardHeartCount +
            activeSceneryCount +
            activeDepthCueCount +
            (player_ ? 1u : 0u) +
            enemies_.size() +
            activeBulletDrawEstimate +
            hitVisualCount;
        const size_t estimatedPooledBulletObjects = pooledBulletCount * 5u;
        historyTime[historyOffset] = static_cast<float>(historyOffset);
        enemyHistory[historyOffset] = static_cast<float>(enemies_.size());
        bulletHistory[historyOffset] =
            static_cast<float>(playerBullets_.size() + enemyBullets_.size());
        frameMsHistory[historyOffset] = frameMs;
        historyOffset = (historyOffset + 1) % 120;

        ImGui::Text("Perf: %.1f FPS / %.2f ms", io.Framerate, frameMs);
        ImGui::Text(
            "Objects: draw est %zu / pooled bullet objects %zu",
            estimatedActiveDrawObjects,
            estimatedPooledBulletObjects);
        ImGui::Text(
            "Active: depth %zu, scenery %zu/%zu, enemies %zu, bullets %zu, hit effects %zu, hit visuals %zu",
            activeDepthCueCount,
            activeSceneryCount,
            totalSceneryCount,
            enemies_.size(),
            activeBulletCount,
            hitEffects_.size(),
            hitVisualCount);
        ImGui::Text(
            "Pools: player %zu, enemy %zu",
            playerBulletPool_.size(),
            enemyBulletPool_.size());
        ImGui::Text(
            "Scene: objects %zu, hearts %zu/%zu miss %zu",
            sceneObjects_.size(),
            activeRewardHeartCount,
            rewardHearts_.size(),
            rewardHeartPoolMisses_);
        ImGui::Separator();
        if (ImPlot::BeginPlot("Frame time", ImVec2(-1.0f, 130.0f))) {
            ImPlot::SetupAxes("frame", "ms", ImPlotAxisFlags_NoTickLabels, 0);
            ImPlot::PlotLine("ms", historyTime, frameMsHistory, 120);
            ImPlot::EndPlot();
        }

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
                { 6, 42, 30.0f },
                { 8, 38, 34.0f },
                { 10, 34, 38.0f }
            } };
            railSpeed_ = 0.145f;
            targetRailSpeed_ = 0.145f;
            playerBulletSpeed_ = 1.36f;
            lockBulletSpeed_ = 1.62f;
            chargedBulletSpeedMultiplier_ = 1.12f;
            enemyBulletSpeed_ = 0.36f;
            lockRadius_ = 118.0f;
            chargeShotThreshold_ = 88;
            normalShootCooldown_ = 17;
            chargedShootCooldown_ = 30;
            enemyShotInterval_ = 64;
            waveStartDelay_ = 90;
            editorStatusMessage_ = "ゲーム調整をリセットしました。";
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_FORWARD_STEP " 今すぐ出現")) {
            enemySpawnTimer_ = 0;
        }

        if (ImGui::CollapsingHeader(ICON_FA_WAND_MAGIC_SPARKLES " 操作感", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("レール速度", &railSpeed_, 0.010f, 0.240f, "%.3f");
            ImGui::SliderFloat("自弾速度", &playerBulletSpeed_, 0.30f, 1.80f, "%.2f");
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
    const int waveEnemyCount = GetTotalEnemyTargetCount();
    const std::string scoreText = std::to_string(score_);
    const std::string waveText =
        "ウェーブ " + std::to_string(waveNumber) + " / " + std::to_string(kWaveCount);
    const std::string enemyText =
        "敵 " + std::to_string(enemies_.size()) + "  出現 " +
        std::to_string((std::min)(spawnedEnemyCountInWave_, waveEnemyCount)) +
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

    DrawBossHud();
    DrawStageCueHud();
    DrawHitEffects();
    DrawLockOnHud();
}

void GameRuntime::DrawBossHud()
{
    const Enemy* boss = GetBossEnemy();
    if (!boss || !bossSpawned_ || bossDefeated_) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    Math::Vector2 hudMin{};
    Math::Vector2 hudSize{};
    GetEffectiveHudViewportRect(hudMin, hudSize);
    const ImVec2 origin(hudMin.x, hudMin.y);
    const ImVec2 drawSize(hudSize.x, hudSize.y);

    const int maxHp = (std::max)(boss->GetMaxHp(), 1);
    const int hp = std::clamp(boss->GetHp(), 0, maxHp);
    const float hpRate = static_cast<float>(hp) / static_cast<float>(maxHp);
    const float panelWidth = std::clamp(drawSize.x * 0.38f, 360.0f, 560.0f);
    const ImVec2 panelMin(
        origin.x + (drawSize.x - panelWidth) * 0.5f,
        origin.y + 18.0f);
    const ImVec2 panelMax(panelMin.x + panelWidth, panelMin.y + 54.0f);
    const ImVec2 barMin(panelMin.x + 18.0f, panelMin.y + 31.0f);
    const ImVec2 barMax(panelMax.x - 18.0f, panelMin.y + 43.0f);
    const ImVec2 barFillMax(
        barMin.x + (barMax.x - barMin.x) * std::clamp(hpRate, 0.0f, 1.0f),
        barMax.y);
    const std::string hpText = std::to_string(hp) + " / " + std::to_string(maxHp);

    drawList->AddRectFilled(
        ImVec2(panelMin.x + 4.0f, panelMin.y + 5.0f),
        ImVec2(panelMax.x + 4.0f, panelMax.y + 5.0f),
        IM_COL32(0, 0, 0, 78),
        6.0f);
    drawList->AddRectFilled(
        panelMin,
        panelMax,
        IM_COL32(10, 8, 20, 188),
        6.0f);
    drawList->AddRect(
        panelMin,
        panelMax,
        IM_COL32(205, 120, 255, 130),
        6.0f);
    drawList->AddText(
        ImVec2(panelMin.x + 16.0f, panelMin.y + 10.0f),
        IM_COL32(238, 222, 255, 245),
        "BOSS");
    drawList->AddText(
        ImVec2(panelMax.x - 16.0f - ImGui::CalcTextSize(hpText.c_str()).x,
               panelMin.y + 10.0f),
        IM_COL32(255, 238, 210, 245),
        hpText.c_str());
    drawList->AddRectFilled(barMin, barMax, IM_COL32(18, 16, 28, 235), 4.0f);
    drawList->AddRectFilled(
        barMin,
        barFillMax,
        hpRate <= 0.35f ?
            IM_COL32(255, 70, 92, 245) :
            IM_COL32(255, 88, 176, 245),
        4.0f);
    drawList->AddRect(barMin, barMax, IM_COL32(250, 230, 255, 120), 4.0f);
}

void GameRuntime::DrawStageCueHud()
{
    if (bossWarningTimer_ <= 0 &&
        bossIntroTimer_ <= 0 &&
        bossDefeatFlashTimer_ <= 0 &&
        justDodgeFlashTimer_ <= 0 &&
        playerImpactFlashTimer_ <= 0) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    Math::Vector2 hudMin{};
    Math::Vector2 hudSize{};
    GetEffectiveHudViewportRect(hudMin, hudSize);
    const ImVec2 origin(hudMin.x, hudMin.y);
    const ImVec2 drawSize(hudSize.x, hudSize.y);
    const ImVec2 center(origin.x + drawSize.x * 0.5f, origin.y + drawSize.y * 0.29f);

    if (justDodgeFlashTimer_ > 0) {
        const float rate =
            static_cast<float>(justDodgeFlashTimer_) /
            static_cast<float>((std::max)(kJustDodgeFlashDuration, 1));
        const float sweep = 1.0f - rate;
        const float easedRate = rate * rate;
        const float hudDodgeLean =
            player_ ? static_cast<float>(player_->GetDodgeDirection() >= 0 ? 1 : -1) : 1.0f;
        const float shear = hudDodgeLean * (28.0f + sweep * 78.0f);
        const int lineAlpha = static_cast<int>(128.0f * easedRate);
        const int echoAlpha = static_cast<int>(72.0f * easedRate);
        const float topY = origin.y + drawSize.y * 0.145f;
        const float bottomY = origin.y + drawSize.y * 0.855f;
        const float leftX = origin.x + drawSize.x * (0.08f + sweep * 0.10f);
        const float rightX = origin.x + drawSize.x * (0.92f - sweep * 0.10f);

        drawList->AddLine(
            ImVec2(leftX + shear, topY),
            ImVec2(leftX + drawSize.x * 0.20f + shear * 0.15f, topY + 7.0f),
            IM_COL32(168, 242, 255, (std::clamp)(lineAlpha, 0, 148)),
            2.2f);
        drawList->AddLine(
            ImVec2(rightX + shear * 0.20f, bottomY - 7.0f),
            ImVec2(rightX - drawSize.x * 0.20f + shear, bottomY),
            IM_COL32(255, 154, 232, (std::clamp)(echoAlpha, 0, 92)),
            1.8f);
        drawList->AddLine(
            ImVec2(origin.x + drawSize.x * 0.14f - shear * 0.30f, bottomY),
            ImVec2(origin.x + drawSize.x * 0.34f - shear, bottomY - 8.0f),
            IM_COL32(168, 242, 255, (std::clamp)(echoAlpha, 0, 92)),
            1.8f);
    }

    if (playerImpactFlashTimer_ > 0) {
        const float rate =
            static_cast<float>(playerImpactFlashTimer_) /
            static_cast<float>((std::max)(playerImpactFlashDuration_, 1));
        const float pulse = rate * rate;
        const int fillAlpha = static_cast<int>(58.0f * pulse);
        const int lineAlpha = static_cast<int>(170.0f * pulse);
        drawList->AddRectFilled(
            origin,
            ImVec2(origin.x + drawSize.x, origin.y + drawSize.y),
            IM_COL32(38, 146, 255, (std::clamp)(fillAlpha, 0, 72)));
        drawList->AddRect(
            ImVec2(origin.x + 10.0f, origin.y + 10.0f),
            ImVec2(origin.x + drawSize.x - 10.0f, origin.y + drawSize.y - 10.0f),
            IM_COL32(148, 228, 255, (std::clamp)(lineAlpha, 0, 180)),
            0.0f,
            0,
            2.0f);
    }

    if (bossDefeatFlashTimer_ > 0) {
        const float rate =
            static_cast<float>(bossDefeatFlashTimer_) /
            static_cast<float>((std::max)(kBossDefeatFlashDuration, 1));
        const int alpha = static_cast<int>(120.0f * rate * rate);
        drawList->AddRectFilled(
            origin,
            ImVec2(origin.x + drawSize.x, origin.y + drawSize.y),
            IM_COL32(255, 174, 76, (std::clamp)(alpha, 0, 120)));
    }

    if (bossWarningTimer_ > 0) {
        const float warningRate =
            static_cast<float>(bossWarningTimer_) /
            static_cast<float>((std::max)(kBossWarningDuration, 1));
        const float pulse = 0.5f + 0.5f * std::sin(cameraTimer_ * 11.0f);
        const float width = std::clamp(drawSize.x * 0.44f, 360.0f, 620.0f);
        const ImVec2 panelMin(center.x - width * 0.5f, center.y - 42.0f);
        const ImVec2 panelMax(center.x + width * 0.5f, center.y + 42.0f);
        const int panelAlpha =
            static_cast<int>(170.0f * (0.58f + 0.42f * pulse) * (std::min)(1.0f, warningRate * 2.2f));

        drawList->AddRectFilled(
            panelMin,
            panelMax,
            IM_COL32(35, 4, 12, (std::clamp)(panelAlpha, 0, 185)),
            5.0f);
        drawList->AddRect(
            panelMin,
            panelMax,
            IM_COL32(255, 72, 86, 220),
            5.0f,
            0,
            2.0f);
        drawList->AddLine(
            ImVec2(panelMin.x + 18.0f, panelMin.y + 16.0f),
            ImVec2(panelMax.x - 18.0f, panelMin.y + 16.0f),
            IM_COL32(255, 194, 92, 190),
            2.0f);
        drawList->AddLine(
            ImVec2(panelMin.x + 18.0f, panelMax.y - 16.0f),
            ImVec2(panelMax.x - 18.0f, panelMax.y - 16.0f),
            IM_COL32(255, 194, 92, 190),
            2.0f);

        const char* warningText =
            bossSpawned_ ? "BOSS APPROACHING" : "WARNING";
        const ImVec2 textSize = ImGui::CalcTextSize(warningText);
        drawList->AddText(
            ImVec2(center.x - textSize.x * 0.5f + 2.0f, center.y - textSize.y * 0.5f + 2.0f),
            IM_COL32(0, 0, 0, 210),
            warningText);
        drawList->AddText(
            ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f),
            IM_COL32(255, 236, 184, 255),
            warningText);
    }

    if (bossIntroTimer_ > 0 && bossSpawned_ && !bossDefeated_) {
        const float introRate =
            static_cast<float>(bossIntroTimer_) /
            static_cast<float>((std::max)(kBossIntroDuration, 1));
        const float sweep = 1.0f - introRate;
        const float lineY = origin.y + drawSize.y * (0.64f + std::sin(cameraTimer_ * 0.8f) * 0.015f);
        const float halfWidth = drawSize.x * (0.08f + 0.38f * sweep);
        const int alpha = static_cast<int>(130.0f * introRate);
        drawList->AddLine(
            ImVec2(center.x - halfWidth, lineY),
            ImVec2(center.x + halfWidth, lineY),
            IM_COL32(255, 72, 142, (std::clamp)(alpha, 0, 150)),
            4.0f);
    }
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
    if (resultTransitionTimer_ > 0) {
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

void GameRuntime::DrawPerformanceOverlay()
{
    if (!isPerformanceOverlayVisible_) {
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();

    size_t activeRewardHeartCount = 0;
    for (const RewardHeart& heart : rewardHearts_) {
        if (heart.isActive && heart.object) {
            ++activeRewardHeartCount;
        }
    }

    size_t hitVisualCount = 0;
    for (const HitEffect& effect : hitEffects_) {
        hitVisualCount += effect.visualCount;
    }

    const size_t activePlayerBulletCount = playerBullets_.size();
    const size_t activeEnemyBulletCount = enemyBullets_.size();
    const size_t activeBulletCount = activePlayerBulletCount + activeEnemyBulletCount;
    const size_t activeBulletDrawEstimate =
        activePlayerBulletCount * 3u +
        activeEnemyBulletCount * 2u;
    const size_t pooledBulletCount = playerBulletPool_.size() + enemyBulletPool_.size();
    const size_t pooledHitEffectObjects = hitEffectObjectPool_.size();
    const size_t activeSceneryCount = visibleSceneryCount_;
    const size_t totalSceneryCount = railSceneryObjects_.size();
    const size_t activeDepthCueCount = depthCueEffects_.size();
    const size_t estimatedActiveDrawObjects =
        sceneObjects_.size() +
        activeRewardHeartCount +
        activeSceneryCount +
        activeDepthCueCount +
        (showSkybox_ && skybox_ ? 1u : 0u) +
        (player_ ? 1u : 0u) +
        enemies_.size() +
        activeBulletDrawEstimate +
        hitVisualCount;
    const size_t estimatedPooledBulletObjects = pooledBulletCount * 5u;
    DirectXCommon::FrameTiming timing{};
    if (dxCommon_) {
        timing = dxCommon_->GetFrameTiming();
    }
    const float measuredFrameMs =
        timing.frameCpuMs > 0.0f ?
        timing.frameCpuMs :
        (io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f);
    const float measuredFps =
        measuredFrameMs > 0.0f ? 1000.0f / measuredFrameMs : 0.0f;

    Math::Vector2 hudMin{};
    Math::Vector2 hudSize{};
    GetEffectiveHudViewportRect(hudMin, hudSize);

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const ImVec2 panelMin(hudMin.x + 18.0f, hudMin.y + hudSize.y - 206.0f);
    const ImVec2 panelMax(panelMin.x + 540.0f, panelMin.y + 186.0f);
    const ImU32 accentColor =
        measuredFrameMs > 28.0f ? IM_COL32(255, 90, 90, 235) :
        measuredFrameMs > 18.0f ? IM_COL32(255, 205, 92, 235) :
                                  IM_COL32(116, 242, 190, 235);

    drawList->AddRectFilled(
        ImVec2(panelMin.x + 3.0f, panelMin.y + 4.0f),
        ImVec2(panelMax.x + 3.0f, panelMax.y + 4.0f),
        IM_COL32(0, 0, 0, 82),
        6.0f);
    drawList->AddRectFilled(panelMin, panelMax, IM_COL32(6, 10, 18, 180), 6.0f);
    drawList->AddRect(panelMin, panelMax, accentColor, 6.0f, 0, 2.0f);

    char line[256]{};
    std::snprintf(
        line,
        sizeof(line),
        "F3 PERF inst %.1f FPS / %.2f ms  avg %.1f FPS",
        measuredFps,
        measuredFrameMs,
        io.Framerate);
    drawList->AddText(ImVec2(panelMin.x + 12.0f, panelMin.y + 10.0f), accentColor, line);

    std::snprintf(
        line,
        sizeof(line),
        "draw est %zu  depth %zu  scenery %zu/%zu  hearts %zu/%zu  bullets %zu  enemies %zu",
        estimatedActiveDrawObjects,
        activeDepthCueCount,
        activeSceneryCount,
        totalSceneryCount,
        activeRewardHeartCount,
        rewardHearts_.size(),
        activeBulletCount,
        enemies_.size());
    drawList->AddText(
        ImVec2(panelMin.x + 12.0f, panelMin.y + 35.0f),
        IM_COL32(224, 238, 248, 230),
        line);

    std::snprintf(
        line,
        sizeof(line),
        "pool P %zu/%d E %zu/%d  miss P/E %zu/%zu  warm %d/%d",
        playerBulletPool_.size(),
        kTargetPlayerBulletPoolCount,
        enemyBulletPool_.size(),
        kTargetEnemyBulletPoolCount,
        playerBulletPoolMisses_,
        enemyBulletPoolMisses_,
        (std::min)(bulletPoolWarmupTimer_, kBulletPoolWarmupStartDelayFrames),
        kBulletPoolWarmupStartDelayFrames);
    drawList->AddText(
        ImVec2(panelMin.x + 12.0f, panelMin.y + 58.0f),
        IM_COL32(188, 210, 228, 220),
        line);

    std::snprintf(
        line,
        sizeof(line),
        "pool objs bullet %zu  hitFx %zu/%d miss %zu  heart miss %zu  post %d%s sky %s",
        estimatedPooledBulletObjects,
        pooledHitEffectObjects,
        kTargetHitEffectObjectPoolCount,
        hitEffectObjectPoolMisses_,
        rewardHeartPoolMisses_,
        GetPostEffectMode(),
        isPostEffectBypassEnabled_ ? " BYPASS" : "",
        showSkybox_ ? "ON" : "OFF");
    drawList->AddText(
        ImVec2(panelMin.x + 12.0f, panelMin.y + 82.0f),
        IM_COL32(188, 210, 228, 220),
        line);

    std::snprintf(
        line,
        sizeof(line),
        "cpu upd %.1f pre %.1f scene %.1f post %.1f total %.1f",
        timing.updateMs,
        timing.preDrawMs,
        timing.sceneDrawMs,
        timing.postEffectMs,
        timing.frameCpuMs);
    drawList->AddText(
        ImVec2(panelMin.x + 12.0f, panelMin.y + 106.0f),
        IM_COL32(206, 224, 238, 224),
        line);

    std::snprintf(
        line,
        sizeof(line),
        "rail %.1f  speed %.3f -> %.3f  section %s / %s",
        railDistance_,
        railSpeed_,
        targetRailSpeed_,
        stageSectionName_ ? stageSectionName_ : "Unknown",
        stageCombatBeatName_ ? stageCombatBeatName_ : "Intro");
    drawList->AddText(
        ImVec2(panelMin.x + 12.0f, panelMin.y + 130.0f),
        IM_COL32(206, 224, 238, 224),
        line);

    std::snprintf(
        line,
        sizeof(line),
        "wait present %.1f fence %.1f fps %.1f  F4 post/F5 sky",
        timing.presentMs,
        timing.fenceWaitMs,
        timing.fpsWaitMs);
    drawList->AddText(
        ImVec2(panelMin.x + 12.0f, panelMin.y + 154.0f),
        IM_COL32(206, 224, 238, 224),
        line);
}

void GameRuntime::UpdatePlayerBullets()
{
    for (auto iterator = playerBullets_.begin();
        iterator != playerBullets_.end();) {
        if ((*iterator)->CanHome()) {
            Bullet* bullet = iterator->get();
            auto targetIterator = homingBulletTargets_.find(bullet);
            const Enemy* target =
                targetIterator != homingBulletTargets_.end() ?
                targetIterator->second :
                nullptr;
            const Math::Vector3 targetPosition =
                target ? target->GetAimPosition() : Math::Vector3{};
            const bool hasPassedHomingTarget =
                target &&
                targetPosition.z + 1.25f < bullet->GetTranslate().z;
            if (target && !target->IsDead() && !hasPassedHomingTarget) {
                (*iterator)->SetHomingTarget(targetPosition);
            } else {
                (*iterator)->ClearHomingTarget();
                homingBulletTargets_.erase(bullet);
            }
        }
        (*iterator)->Update();
        if ((*iterator)->IsDead()) {
            homingBulletTargets_.erase(iterator->get());
            playerBulletPool_.push_back(std::move(*iterator));
            iterator = playerBullets_.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

void GameRuntime::UpdateEnemyBullets()
{
    const float worldTimeScale = GetCinematicWorldTimeScale();
    for (auto iterator = enemyBullets_.begin();
        iterator != enemyBullets_.end();) {
        Bullet* bullet = iterator->get();
        bullet->Update(worldTimeScale);
        if (bullet->IsDead()) {
            justDodgedEnemyBullets_.erase(bullet);
            enemyBulletPool_.push_back(std::move(*iterator));
            iterator = enemyBullets_.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

void GameRuntime::UpdateEnemies()
{
    const float worldTimeScale = GetCinematicWorldTimeScale();
    for (auto iterator = enemies_.begin(); iterator != enemies_.end();) {
        (*iterator)->Update(railDistance_, worldTimeScale);
        if ((*iterator)->IsDead()) {
            const Enemy* removedEnemy = iterator->get();
            for (auto targetIterator = homingBulletTargets_.begin();
                targetIterator != homingBulletTargets_.end();) {
                if (targetIterator->second == removedEnemy) {
                    targetIterator = homingBulletTargets_.erase(targetIterator);
                } else {
                    ++targetIterator;
                }
            }
            iterator = enemies_.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

float GameRuntime::GetCinematicWorldTimeScale() const
{
    float timeScale = 1.0f;
    if (justDodgeSlowTimer_ > 0) {
        timeScale = (std::min)(timeScale, kJustDodgeRailSlowScale);
    }
    if (playerImpactSlowTimer_ > 0) {
        timeScale *= playerImpactSlowScale_;
    }
    return std::clamp(timeScale, 0.05f, 1.0f);
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
        if (!enemy || enemy->IsDead() || !enemy->IsTargetable()) {
            continue;
        }
        const Math::Vector3 enemyPosition = enemy->GetAimPosition();
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
    return static_cast<int>(kStageEnemyEventCount);
}

int GameRuntime::GetRequiredEnemyDefeatsForClear() const
{
    const int totalEnemyCount = GetTotalEnemyTargetCount();
    return (std::max)(1, (totalEnemyCount * 7 + 9) / 10);
}

const Enemy* GameRuntime::GetBossEnemy() const
{
    for (const auto& enemy : enemies_) {
        if (enemy && enemy->IsBoss() && !enemy->IsDead()) {
            return enemy.get();
        }
    }
    return nullptr;
}

void GameRuntime::CheckBulletEnemyCollisions()
{
    for (auto& bullet : playerBullets_) {
        if (bullet->IsDead()) {
            continue;
        }

        for (auto& enemy : enemies_) {
            if (enemy->IsDead() || !enemy->IsTargetable()) {
                continue;
            }

            const Math::Vector3 enemyAimPosition = enemy->GetAimPosition();
            const float radius = bullet->GetRadius() + enemy->GetAimRadius();
            if (DistanceSquared(
                bullet->GetTranslate(),
                enemyAimPosition) <= radius * radius) {
                const bool isChargedHit = bullet->GetRadius() >= 0.8f;
                const bool isBossHit = enemy->IsBoss();
                const int damage = isChargedHit ? (isBossHit ? 4 : 3) : 1;
                const Math::Vector3 bulletImpactPosition = bullet->GetTranslate();
                const float centerEffectRate = isBossHit ? 0.68f : 0.42f;
                const Math::Vector3 visibleImpactPosition{
                    Lerp(bulletImpactPosition.x, enemyAimPosition.x, centerEffectRate),
                    Lerp(bulletImpactPosition.y, enemyAimPosition.y, centerEffectRate),
                    Lerp(bulletImpactPosition.z, enemyAimPosition.z, centerEffectRate)
                };
                AddEnemyImpactEffect(
                    visibleImpactPosition,
                    isBossHit ? 1.85f : (isChargedHit ? 1.42f : 1.20f));
                bullet->RegisterHit();
                const bool isDestroyed = enemy->Damage(damage);
                AddCameraShake(
                    isDestroyed ?
                        (isBossHit ? 0.18f : (isChargedHit ? 0.075f : 0.055f)) :
                        (isBossHit ? 0.035f : 0.022f),
                    isDestroyed ?
                        (isBossHit ? 30 : (isChargedHit ? 11 : 8)) :
                        (isBossHit ? 6 : 4));
                TriggerPlayerImpactMoment(isChargedHit, isBossHit, isDestroyed);
                if (isDestroyed) {
                    AddEnemyHitEffect(
                        enemyAimPosition,
                        isBossHit ? 2.0f : (isChargedHit ? 1.32f : 1.0f));
                    SpawnRewardHearts(
                        enemyAimPosition,
                        isBossHit ? 14 : (isChargedHit ? 6 : 4));
                    score_ += isBossHit ? 1500 : 100;
                    ++defeatedEnemyCount_;
                    ++defeatedEnemyCountInWave_;
                    if (isBossHit) {
                        bossDefeatFlashTimer_ = kBossDefeatFlashDuration;
                        AddEnemyHitEffect(
                            { enemyAimPosition.x - 2.2f, enemyAimPosition.y + 0.5f, enemyAimPosition.z - 0.8f },
                            1.55f);
                        AddEnemyHitEffect(
                            { enemyAimPosition.x + 2.2f, enemyAimPosition.y - 0.3f, enemyAimPosition.z + 0.4f },
                            1.45f);
                        AddEnemyHitEffect(
                            { enemyAimPosition.x, enemyAimPosition.y + 1.2f, enemyAimPosition.z + 1.1f },
                            1.35f);
                        AddCameraShake(0.34f, 58);
                        bossDefeated_ = true;
                        currentWaveIndex_ = kWaveCount - 1;
                        isGameClear_ = true;
                        resultTransitionTimer_ = 120;
                        stageCombatBeatName_ = "Boss destroyed";
                    }
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
        const Math::Vector3 bulletPosition = bullet->GetTranslate();
        const float distanceSq = DistanceSquared(
            bulletPosition,
            player_->GetTranslate());
        if (player_->IsDodging()) {
            const float justDodgeRadius = radius + kJustDodgeGrazePadding;
            if (distanceSq > radius * radius &&
                distanceSq <= justDodgeRadius * justDodgeRadius) {
                TriggerJustDodge(*bullet, bulletPosition);
                continue;
            }
        }
        if (distanceSq <= radius * radius) {
            if (player_->IsDodging()) {
                bullet->Kill();
                AddPlayerDodgeGrazeEffect(bulletPosition);
                AddCameraShake(0.018f, 4);
                continue;
            }
            bullet->Kill();
            player_->Damage(8);
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

    const float frameStep =
        dxCommon_ ? std::clamp(dxCommon_->GetDeltaTime() * 60.0f, 0.5f, 2.0f) : 1.0f;
    cameraTimer_ += frameStep;

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
    const float railCurve =
        std::sin(railDistance_ * kRailCameraCurveFrequency);
    const float railDrift =
        std::sin(railDistance_ * kRailCameraDriftFrequency + 1.35f);
    const float railLift =
        std::sin(railDistance_ * 0.034f + 0.45f) * 0.18f;
    const float railWideCurve =
        railCurve * 0.32f +
        railDrift * 0.10f +
        std::sin(railDistance_ * 0.014f + 2.20f) * 0.14f;
    const float turnRate = std::clamp(railWideCurve, -0.65f, 0.65f);
    const float speedPulse =
        0.5f + 0.5f * std::sin(railDistance_ * 0.021f + 0.80f);
    const float railSpeedRate = std::clamp((railSpeed_ - 0.13f) / 0.10f, 0.0f, 1.0f);
    const bool isPlayerDodging = player_->IsDodging();
    const float justDodgeCameraRate =
        justDodgeFlashTimer_ > 0 ?
        static_cast<float>(justDodgeFlashTimer_) /
            static_cast<float>((std::max)(kJustDodgeFlashDuration, 1)) :
        0.0f;
    const float justDodgeImpulse =
        justDodgeCameraRate > 0.0f ?
        std::sin((1.0f - justDodgeCameraRate) * std::numbers::pi_v<float>) :
        0.0f;
    const float justDodgeWhip = justDodgeCameraRate * justDodgeCameraRate;
    const float justDodgeRipple =
        justDodgeCameraRate > 0.0f ?
        std::sin((1.0f - justDodgeCameraRate) * std::numbers::pi_v<float> * 2.0f) *
            justDodgeCameraRate :
        0.0f;
    const float justDodgeChromaticShift =
        justDodgeImpulse * (0.35f + justDodgeCameraRate * 0.65f);
    const float playerImpactCameraRate =
        playerImpactFlashTimer_ > 0 ?
        static_cast<float>(playerImpactFlashTimer_) /
            static_cast<float>((std::max)(playerImpactFlashDuration_, 1)) :
        0.0f;
    const float edgeRollRate = std::clamp(
        (std::abs(playerTranslate.x) - kGameplayCameraEdgeRollStart) /
            kGameplayCameraEdgeRollRange,
        0.0f,
        1.0f);
    const float smoothEdgeRollRate =
        edgeRollRate * edgeRollRate * (3.0f - 2.0f * edgeRollRate);
    const float edgeDirection =
        playerTranslate.x > 0.0f ? 1.0f :
        playerTranslate.x < 0.0f ? -1.0f :
        0.0f;
    const float playerEdgeRoll =
        edgeDirection * smoothEdgeRollRate * kGameplayCameraEdgeRollMax *
        (1.0f - justDodgeWhip * 0.92f);
    const float dodgeLean =
        isPlayerDodging ?
        static_cast<float>(player_->GetDodgeDirection() >= 0 ? 1 : -1) :
        0.0f;
    const float inputSpeed = std::clamp(
        std::abs(playerVelocity.x) * 4.2f +
            std::abs(playerVelocity.y) * 2.8f,
        0.0f,
        1.0f);
    const float shakeX = std::sin(cameraTimer_ * 1.9f) * cameraShakePower_ * shakeRate;
    const float shakeY = std::cos(cameraTimer_ * 2.3f) * cameraShakePower_ * shakeRate;
    const float resultZoom =
        isGameClear_ ? 0.9f :
        isGameOver_ ? -0.8f :
        0.0f;

    const Math::Vector3 targetTranslate = {
            playerTranslate.x * (0.08f + justDodgeWhip * kJustDodgeCameraPlayerFollowX) +
            playerVelocity.x * 0.55f +
            dodgeLean * 0.18f +
            dodgeLean * (justDodgeWhip * 0.78f + justDodgeRipple * 0.22f) +
            std::sin(cameraTimer_ * 3.7f) * justDodgeChromaticShift * 0.085f +
            railWideCurve * 0.22f +
            railDrift * 0.04f +
            shakeX,
        2.65f +
            playerTranslate.y * (0.04f + justDodgeWhip * kJustDodgeCameraPlayerFollowY) +
            playerVelocity.y * 0.38f +
            railLift +
            stageCameraLiftBias_ +
            std::sin(railDistance_ * 0.017f + 0.35f) * 0.16f +
            justDodgeImpulse * 0.24f +
            justDodgeWhip * (0.08f - kJustDodgeCameraLowAngle) +
            bob +
            shakeY,
            railDistance_ -
            (kGameplayCameraBaseDistance +
                speedPulse * 0.55f +
                railSpeedRate * 0.46f +
                inputSpeed * 0.22f +
            std::abs(turnRate) * 0.18f) +
            resultZoom +
            justDodgeWhip * kJustDodgeCameraClosePushIn +
            justDodgeImpulse * 0.36f +
            playerImpactCameraRate * playerImpactCameraRate * 0.42f,
    };

    const float cameraTranslateRate =
        0.065f +
        justDodgeCameraRate * 0.105f +
        playerImpactCameraRate * 0.020f;
    cameraTranslate_ = Lerp(cameraTranslate_, targetTranslate, cameraTranslateRate);

    const Math::Vector3 targetRotate = {
        0.065f +
            playerTranslate.y * 0.003f +
            railLift * 0.005f +
            speedPulse * 0.004f +
            bob * 0.006f +
            shakeY * 0.008f,
        -playerTranslate.x * 0.0045f +
            railWideCurve * 0.018f +
            railDrift * 0.004f +
            stageCameraYawBias_ * 0.35f +
            playerVelocity.x * 0.006f,
        stageCameraRollBias_ * 0.55f -
            turnRate * 0.024f +
            playerEdgeRoll +
            playerImpactCameraRate * 0.010f +
            shakeX * 0.006f,
    };

    const float targetFov =
        kGameplayCameraBaseFovY +
        speedPulse * 0.040f +
        railSpeedRate * 0.035f +
        std::abs(turnRate) * 0.020f +
        inputSpeed * 0.025f +
        (isPlayerDodging ? 0.024f : 0.0f) +
        playerImpactCameraRate * 0.030f -
        justDodgeWhip * kJustDodgeCameraFovTighten -
        justDodgeImpulse * 0.026f +
        stageCameraFovBoost_ +
        (isGameClear_ ? -0.035f : 0.0f) +
        (isGameOver_ ? 0.025f : 0.0f) +
        cameraShakePower_ * shakeRate * 0.04f;

    cameraRotate_ = Lerp(cameraRotate_, targetRotate, 0.075f + justDodgeCameraRate * 0.082f);
    cameraFovY_ = Lerp(cameraFovY_, targetFov, 0.060f + justDodgeCameraRate * 0.080f);

    camera_->SetTranslate(cameraTranslate_);
    camera_->SetRotate(cameraRotate_);
    camera_->SetFovY(cameraFovY_);
    camera_->Update();
}

void GameRuntime::AddCameraShake(float power, int duration)
{
    cameraShakePower_ = (std::max)(cameraShakePower_, power);
    cameraShakeDuration_ = (std::max)(duration, 1);
    cameraShakeTimer_ = (std::max)(cameraShakeTimer_, cameraShakeDuration_);
}
