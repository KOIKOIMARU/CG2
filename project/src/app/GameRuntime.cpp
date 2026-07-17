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
#include <chrono>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <string>
#include <string_view>

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
constexpr int kDepthCueEffectCount = 18;
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
constexpr float kStageTimelineCruiseSpeed = 0.090f;
constexpr int kStageEncounterBreatherFrames = 84;
constexpr float kStageEncounterGroupingDistance = 10.5f;
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
constexpr int kEnemyVolleyTelegraphLeadFrames = 18;
constexpr int kSniperTelegraphLeadFrames = 34;
constexpr int kFeverGaugeMax = 100;
constexpr int kFeverDurationFrames = 600;
constexpr int kFeverActivationFlashFrames = 90;
constexpr int kFeverRapidShotCooldown = 12;
constexpr int kFeverScoreMultiplier = 3;
constexpr int kFeverEncounterBreatherFrames = 18;
constexpr float kFeverRailSpeedMultiplier = 2.15f;
constexpr float kFeverRailAccelerationResponse = 0.15f;
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
    {   0.0f,  42.0f, 0.160f,  0.000f,  0.000f,  0.00f, 0.048f, "Opening" },
    {  42.0f, 104.0f, 0.205f, -0.022f,  0.042f,  0.16f, 0.068f, "Side sweep" },
    { 104.0f, 170.0f, 0.185f,  0.018f, -0.036f,  0.34f, 0.062f, "Climb and dodge" },
    { 170.0f, 230.0f, 0.235f,  0.000f, -0.055f, -0.16f, 0.102f, "Fast dive" },
    { 230.0f, 999.0f, 0.198f,  0.018f,  0.034f,  0.08f, 0.068f, "Final approach" }
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
    {  78.0f, -3.2f,  1.3f, 48.0f, Enemy::Behavior::Sniper,         Enemy::EntryStyle::PopShooter, 0.052f, 10,  6, 1.18f, "Sniper lock" },
    {  98.0f,  3.2f,  0.9f, 46.0f, Enemy::Behavior::StrafeShooter, Enemy::EntryStyle::PopShooter, 0.042f,  8,  7, 1.16f, "Dodge target" },
    { 112.0f,  4.6f,  1.6f, 54.0f, Enemy::Behavior::DiveBomber,    Enemy::EntryStyle::RightSweep, 0.036f,  8,  5, 1.12f, "High dive" },
    { 124.0f, -2.9f, -0.4f, 44.0f, Enemy::Behavior::Formation,     Enemy::EntryStyle::VFormation, 0.024f,  5,  5, 1.12f, "Guard pair" },
    { 134.0f,  2.9f, -0.2f, 44.0f, Enemy::Behavior::Formation,     Enemy::EntryStyle::VFormation, 0.024f,  5,  5, 1.12f, "Guard pair" },
    { 148.0f, -4.6f,  1.5f, 54.0f, Enemy::Behavior::DiveBomber,    Enemy::EntryStyle::LeftSweep,  0.036f,  8,  5, 1.12f, "High dive" },
    { 160.0f,  0.0f,  1.45f, 54.0f, Enemy::Behavior::Shield,        Enemy::EntryStyle::Direct,     0.070f, 14, 11, 1.24f, "Shield wall" },
    { 174.0f, -5.4f,  0.55f, 52.0f, Enemy::Behavior::Crossfire,     Enemy::EntryStyle::LeftSweep,  0.048f,  9,  6, 1.12f, "Crossfire pair" },
    { 174.0f,  5.4f,  1.15f, 52.0f, Enemy::Behavior::Crossfire,     Enemy::EntryStyle::RightSweep, 0.048f,  9,  6, 1.12f, "Crossfire pair" },
    { 188.0f, -4.9f,  1.0f, 52.0f, Enemy::Behavior::Swoop,         Enemy::EntryStyle::LeftSweep,  0.038f,  7,  5, 1.14f, "High-speed pass" },
    { 202.0f,  4.9f,  1.0f, 52.0f, Enemy::Behavior::Swoop,         Enemy::EntryStyle::RightSweep, 0.038f,  7,  5, 1.14f, "High-speed pass" },
    { 214.0f,  0.0f,  1.0f, 50.0f, Enemy::Behavior::Formation,     Enemy::EntryStyle::VFormation, 0.030f,  6,  5, 1.18f, "Break formation" },
    { 228.0f, -3.8f,  0.6f, 48.0f, Enemy::Behavior::StrafeShooter, Enemy::EntryStyle::PopShooter, 0.040f,  8,  7, 1.18f, "Final crossfire" },
    { 248.0f,  3.8f,  0.6f, 48.0f, Enemy::Behavior::StrafeShooter, Enemy::EntryStyle::PopShooter, 0.040f,  8,  7, 1.18f, "Final crossfire" },
    { 258.0f,  0.0f,  1.9f, 48.0f, Enemy::Behavior::Support,       Enemy::EntryStyle::Direct,     0.050f, 10,  8, 1.16f, "Support gate" },
    { 258.0f, -2.9f, -0.5f, 44.0f, Enemy::Behavior::Formation,     Enemy::EntryStyle::VFormation, 0.030f,  6,  5, 1.16f, "Support gate" },
    { 258.0f,  2.9f, -0.5f, 44.0f, Enemy::Behavior::Formation,     Enemy::EntryStyle::VFormation, 0.030f,  6,  5, 1.16f, "Support gate" },
    { 274.0f,  2.9f, -0.5f, 44.0f, Enemy::Behavior::Formation,     Enemy::EntryStyle::VFormation, 0.030f,  6,  5, 1.16f, "Last gate" },
    { 286.0f, -5.2f,  1.2f, 54.0f, Enemy::Behavior::Swoop,         Enemy::EntryStyle::LeftSweep,  0.034f,  7,  5, 1.12f, "Boss screen" },
    { 296.0f,  5.2f,  1.2f, 54.0f, Enemy::Behavior::Swoop,         Enemy::EntryStyle::RightSweep, 0.034f,  7,  5, 1.12f, "Boss screen" }
};

constexpr size_t kStageEnemyEventCapacity = 28;
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
    stageProgress_ = 0.0f;
    stageTimelineSpeed_ = 0.0f;
    stageTimelineWasBlocked_ = false;
    stageEncounterBreatherTimer_ = 0;
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
    feverGauge_ = 0;
    feverTimer_ = 0;
    feverActivationFlashTimer_ = 0;
    feverSpeedEffectRate_ = 0.0f;
    playerDodgeAfterimageTimer_ = 0;
    justDodgeFlashTimer_ = 0;
    justDodgeSlowTimer_ = 0;
    playerImpactFlashTimer_ = 0;
    playerImpactFlashDuration_ = 1;
    playerImpactSlowTimer_ = 0;
    playerImpactSlowDuration_ = 1;
    playerImpactSlowScale_ = 1.0f;
    hitConfirmTimer_ = 0;
    hitConfirmDuration_ = 1;
    hitConfirmComboTimer_ = 0;
    hitConfirmComboCount_ = 0;
    hitConfirmStrength_ = 1.0f;
    hitConfirmCharged_ = false;
    hitConfirmBoss_ = false;
    hitConfirmDestroyed_ = false;
    playerDamageHudTimer_ = 0;
    playerDamageHudDuration_ = 1;
    playerDamageScreen_ = {};
    playerDamageDirection_ = { 0.0f, -1.0f };
    enemyVolleyTelegraphTriggered_ = false;
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

    UpdateBulletPoolWarmup();
    UpdateHitEffectObjectPoolWarmup();
    UpdateRailProgress();
    UpdatePlayerAndCamera();
    UpdateFever();

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
        if (stageProgress_ >= segment.startDistance &&
            stageProgress_ < segment.endDistance) {
            activeSegment = &segment;
            break;
        }
    }

    stageSectionName_ = activeSegment->name;
    currentWaveIndex_ =
        stageProgress_ < 104.0f ? 0 :
        stageProgress_ < 230.0f ? 1 :
        2;
    targetRailSpeed_ = activeSegment->targetSpeed *
        (feverTimer_ > 0 ? kFeverRailSpeedMultiplier : 1.0f);
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
            stageProgress_ < event.distance) {
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
        stageProgress_ >= kBossWarningDistance) {
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
        railSpeed_ = Lerp(
            railSpeed_,
            targetRailSpeed_,
            feverTimer_ > 0 ? kFeverRailAccelerationResponse : 0.070f);
        const float worldTimeScale = GetCinematicWorldTimeScale();
        railDistance_ += railSpeed_ * worldTimeScale;

        int activeStageEnemyCount = 0;
        for (const auto& enemy : enemies_) {
            if (enemy && !enemy->IsDead() && !enemy->IsBoss()) {
                ++activeStageEnemyCount;
            }
        }

        const StageEnemySpawnEvent* nextEnemyEvent = nullptr;
        const size_t eventCount =
            (std::min)(stageEnemyEventTriggered_.size(), kStageEnemyEventCount);
        for (size_t index = 0; index < eventCount; ++index) {
            if (!stageEnemyEventTriggered_[index]) {
                nextEnemyEvent = &kStageEnemySpawnEvents[index];
                break;
            }
        }

        const bool nextEventContinuesCurrentEncounter =
            nextEnemyEvent != nullptr &&
            std::string_view(nextEnemyEvent->beatName) ==
                std::string_view(stageCombatBeatName_ ? stageCombatBeatName_ : "Intro") &&
            nextEnemyEvent->distance - stageProgress_ <=
                kStageEncounterGroupingDistance;
        const bool currentEncounterFullyScheduled =
            !nextEventContinuesCurrentEncounter;
        const bool timelineBlocked =
            activeStageEnemyCount > 0 && currentEncounterFullyScheduled;

        if (stageTimelineWasBlocked_ && !timelineBlocked &&
            activeStageEnemyCount == 0) {
            stageEncounterBreatherTimer_ = feverTimer_ > 0 ?
                kFeverEncounterBreatherFrames : kStageEncounterBreatherFrames;
        }
        stageTimelineWasBlocked_ = timelineBlocked;

        if (bossSpawned_ || timelineBlocked) {
            stageTimelineSpeed_ = 0.0f;
        } else if (stageEncounterBreatherTimer_ > 0) {
            --stageEncounterBreatherTimer_;
            stageTimelineSpeed_ = 0.0f;
        } else {
            stageTimelineSpeed_ = kStageTimelineCruiseSpeed;
        }
        stageProgress_ += stageTimelineSpeed_ * worldTimeScale;
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
        shootCooldown_ = feverTimer_ > 0 ?
            kFeverRapidShotCooldown :
            (isCharged ? chargedShootCooldown_ : normalShootCooldown_);
    } else if (!isShootPressed && shootBufferTimer_ <= 0) {
        chargeTimer_ = (std::min)(chargeTimer_ + 1, kChargeShotMax);
    }

    if (chargeFlashTimer_ > 0) {
        --chargeFlashTimer_;
    }
}

void GameRuntime::UpdateFever()
{
    const float targetSpeedEffectRate = feverTimer_ > 0 ? 1.0f : 0.0f;
    const float speedEffectResponse =
        targetSpeedEffectRate > feverSpeedEffectRate_ ? 0.16f : 0.045f;
    feverSpeedEffectRate_ = Lerp(
        feverSpeedEffectRate_,
        targetSpeedEffectRate,
        speedEffectResponse);
    if (targetSpeedEffectRate <= 0.0f && feverSpeedEffectRate_ < 0.001f) {
        feverSpeedEffectRate_ = 0.0f;
    }

    if (feverActivationFlashTimer_ > 0) {
        --feverActivationFlashTimer_;
    }

    if (feverTimer_ > 0) {
        --feverTimer_;
        chargeTimer_ = kChargeShotMax;
        return;
    }

    if (isGameOver_ || isGameClear_ || !input_ ||
        feverGauge_ < kFeverGaugeMax || !input_->TriggerKey(DIK_E)) {
        return;
    }

    ActivateFever();
}

void GameRuntime::ActivateFever()
{
    if (isGameOver_ || isGameClear_) {
        return;
    }

    feverGauge_ = 0;
    feverTimer_ = kFeverDurationFrames;
    feverActivationFlashTimer_ = kFeverActivationFlashFrames;
    chargeTimer_ = kChargeShotMax;
    chargeFlashTimer_ = (std::max)(chargeFlashTimer_, 30);
    stageEncounterBreatherTimer_ =
        (std::min)(stageEncounterBreatherTimer_, kFeverEncounterBreatherFrames);
    if (player_) {
        const Math::Vector3 playerPosition = player_->GetTranslate();
        AddPlayerDodgeGrazeEffect(playerPosition);
        AddPlayerDodgeGrazeEffect({
            playerPosition.x,
            playerPosition.y + 0.18f,
            playerPosition.z + 0.70f
        });
    }
    AddCameraShake(0.16f, 24);
}

void GameRuntime::AddFeverGauge(int amount)
{
    if (amount <= 0 || feverTimer_ > 0 || isGameOver_ || isGameClear_) {
        return;
    }
    feverGauge_ = (std::clamp)(feverGauge_ + amount, 0, kFeverGaugeMax);
}

void GameRuntime::AddScore(int baseScore)
{
    if (baseScore <= 0) {
        return;
    }
    score_ += baseScore * (feverTimer_ > 0 ? kFeverScoreMultiplier : 1);
}

#ifdef ENABLE_DEBUG_GUI
void GameRuntime::DebugJumpToStagePhase(int phaseIndex)
{
    if (isGameOver_ || isGameClear_) {
        editorStatusMessage_ = "フェーズ移動はゲーム進行中のみ使用できます。";
        return;
    }

    constexpr std::array<float, 4> kDebugPhaseStartDistances{
        0.0f,
        104.0f,
        230.0f,
        kBossSpawnDistance
    };
    const int clampedPhase =
        (std::clamp)(phaseIndex, 0, static_cast<int>(kDebugPhaseStartDistances.size()) - 1);
    const float targetProgress = kDebugPhaseStartDistances[clampedPhase];

    lockedEnemy_ = nullptr;
    hasLockTarget_ = false;
    isReticleOnTarget_ = false;
    homingBulletTargets_.clear();
    justDodgedEnemyBullets_.clear();
    enemies_.clear();

    while (!playerBullets_.empty()) {
        playerBulletPool_.push_back(std::move(playerBullets_.front()));
        playerBullets_.pop_front();
    }
    while (!enemyBullets_.empty()) {
        enemyBulletPool_.push_back(std::move(enemyBullets_.front()));
        enemyBullets_.pop_front();
    }
    for (RewardHeart& heart : rewardHearts_) {
        heart.isActive = false;
    }

    stageProgress_ = targetProgress;
    stageTimelineSpeed_ = 0.0f;
    stageTimelineWasBlocked_ = false;
    stageEncounterBreatherTimer_ = 0;
    currentWaveIndex_ = (std::min)(clampedPhase, kWaveCount - 1);
    spawnedEnemyCountInWave_ = 0;
    defeatedEnemyCountInWave_ = 0;
    spawnSequenceIndex_ = 0;
    defeatedEnemyCount_ = 0;
    enemySpawnTimer_ = 0;
    enemyShotTimer_ = 32;
    enemyVolleyTelegraphTriggered_ = false;
    bossSpawned_ = false;
    bossDefeated_ = false;
    bossWarningTriggered_ = clampedPhase == 3;
    bossWarningTimer_ = clampedPhase == 3 ? 42 : 0;
    bossIntroTimer_ = 0;
    bossDefeatFlashTimer_ = 0;
    resultTransitionTimer_ = -1;

    const size_t railEventCount =
        (std::min)(
            stageRailEventTriggered_.size(),
            sizeof(kStageRailEvents) / sizeof(kStageRailEvents[0]));
    for (size_t index = 0; index < railEventCount; ++index) {
        stageRailEventTriggered_[index] =
            kStageRailEvents[index].distance < targetProgress;
    }

    const size_t enemyEventCount =
        (std::min)(stageEnemyEventTriggered_.size(), kStageEnemyEventCount);
    for (size_t index = 0; index < enemyEventCount; ++index) {
        const bool skippedEvent =
            clampedPhase == 3 ||
            kStageEnemySpawnEvents[index].distance < targetProgress;
        stageEnemyEventTriggered_[index] = skippedEvent;
        if (skippedEvent) {
            ++spawnSequenceIndex_;
            ++defeatedEnemyCount_;
        }
    }

    stageCombatBeatName_ = clampedPhase == 3 ? "Boss" : "Debug phase jump";
    UpdateStageDirector();

    constexpr const char* kDebugPhaseNames[] = {
        "Wave 1",
        "Wave 2",
        "Wave 3",
        "Boss"
    };
    editorStatusMessage_ =
        std::string("デバッグ移動: ") + kDebugPhaseNames[clampedPhase];
}
#endif

void GameRuntime::UpdateEnemyActions()
{
    if (justDodgeSlowTimer_ > 0) {
        return;
    }

    UpdateEnemyWave();

    bool hasSupportDrone = false;
    for (const auto& enemy : enemies_) {
        if (enemy && !enemy->IsDead() && enemy->IsSupport()) {
            hasSupportDrone = true;
            break;
        }
    }

    --enemyShotTimer_;
    if (!enemyVolleyTelegraphTriggered_ &&
        enemyShotTimer_ <= kEnemyVolleyTelegraphLeadFrames) {
        int normalTelegraphCount = 0;
        constexpr int kMaxNormalTelegraphs = 2;
        for (const auto& enemy : enemies_) {
            if (!enemy->CanShoot()) {
                continue;
            }

            const Math::Vector3 enemyAimPosition = enemy->GetAimPosition();
            if (enemy->IsBoss()) {
                AddEnemyShotTelegraphEffect({
                    enemyAimPosition.x - 2.2f,
                    enemyAimPosition.y + 0.25f,
                    enemyAimPosition.z - 1.0f
                }, true);
                AddEnemyShotTelegraphEffect({
                    enemyAimPosition.x + 2.2f,
                    enemyAimPosition.y + 0.25f,
                    enemyAimPosition.z - 1.0f
                }, true);
            } else if (normalTelegraphCount < kMaxNormalTelegraphs) {
                AddEnemyShotTelegraphEffect({
                    enemyAimPosition.x,
                    enemyAimPosition.y,
                    enemyAimPosition.z - 1.0f
                }, false);
                ++normalTelegraphCount;
            }
        }
        enemyVolleyTelegraphTriggered_ = true;
    }
    if (enemyShotTimer_ <= 0) {
        int normalEnemyShotsThisVolley = 0;
        const int maxNormalEnemyShotsThisVolley = hasSupportDrone ? 3 : 2;
        for (const auto& enemy : enemies_) {
            if (enemy->IsSniper() && enemy->CanShoot() &&
                normalEnemyShotsThisVolley < maxNormalEnemyShotsThisVolley) {
                FireEnemyBullet(
                    enemy->GetAimPosition(),
                    EnemyBulletStyle::Sniper);
                ++normalEnemyShotsThisVolley;
                break;
            }
        }
        for (const auto& enemy : enemies_) {
            if (enemy->IsCrossfire() && enemy->CanShoot() &&
                normalEnemyShotsThisVolley < maxNormalEnemyShotsThisVolley) {
                const Math::Vector3 enemyAimPosition = enemy->GetAimPosition();
                FireEnemyBullet(
                    enemyAimPosition,
                    EnemyBulletStyle::Crossfire,
                    { enemyAimPosition.x < 0.0f ? 1.25f : -1.25f, 0.0f });
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
                    }, EnemyBulletStyle::BossCannon, { -1.15f, 0.0f });
                    FireEnemyBullet({
                        enemyAimPosition.x + 2.2f,
                        enemyAimPosition.y + 0.25f,
                        enemyAimPosition.z
                    }, EnemyBulletStyle::BossCannon, { 1.15f, 0.0f });
                    if ((enemyShotTimer_ + static_cast<int>(railDistance_ * 10.0f)) % 2 == 0) {
                        FireEnemyBullet({
                            enemyAimPosition.x,
                            enemyAimPosition.y - 0.55f,
                            enemyAimPosition.z
                        }, EnemyBulletStyle::BossCannon, { 0.0f, -1.35f });
                    }
                } else if (enemy->IsShield() &&
                    normalEnemyShotsThisVolley < maxNormalEnemyShotsThisVolley) {
                    FireEnemyBullet(
                        enemyAimPosition,
                        EnemyBulletStyle::ShieldOrb,
                        { -3.0f, 0.15f });
                    FireEnemyBullet(
                        enemyAimPosition,
                        EnemyBulletStyle::ShieldOrb,
                        { 0.0f, -0.25f });
                    FireEnemyBullet(
                        enemyAimPosition,
                        EnemyBulletStyle::ShieldOrb,
                        { 3.0f, 0.15f });
                    ++normalEnemyShotsThisVolley;
                } else if (!enemy->IsCrossfire() && !enemy->IsSniper() &&
                    normalEnemyShotsThisVolley < maxNormalEnemyShotsThisVolley) {
                    FireEnemyBullet(
                        enemyAimPosition,
                        EnemyBulletStyle::Standard);
                    ++normalEnemyShotsThisVolley;
                }
            }
        }
        enemyShotTimer_ = (std::max)(
            24,
            enemyShotInterval_ - (hasSupportDrone ? 18 : 0));
        enemyVolleyTelegraphTriggered_ = false;
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
    if (hitConfirmTimer_ > 0) {
        --hitConfirmTimer_;
        if (hitConfirmTimer_ <= 0) {
            hitConfirmDuration_ = 1;
        }
    }
    if (hitConfirmComboTimer_ > 0) {
        --hitConfirmComboTimer_;
        if (hitConfirmComboTimer_ <= 0) {
            hitConfirmComboCount_ = 0;
        }
    }
    if (playerDamageHudTimer_ > 0) {
        --playerDamageHudTimer_;
        if (playerDamageHudTimer_ <= 0) {
            playerDamageHudDuration_ = 1;
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
                AddScore(heart.scoreValue);
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
        const bool isStreak = (index % 3) != 2 && streakModel;
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
            Math::Vector4{ 0.72f, 0.96f, 1.0f, 0.26f } :
            isGold ?
                Math::Vector4{ 1.0f, 0.86f, 0.42f, 0.40f } :
                Math::Vector4{ 0.78f, 0.96f, 1.0f, 0.34f };
        cue.loopLength = kDepthCueLoopLength;
        cue.speedMultiplier = isStreak ?
            Lerp(1.42f, 2.62f, PseudoRandom01(index, 1.12f)) :
            Lerp(0.86f, 1.58f, PseudoRandom01(index, 1.12f));
        cue.lateralDrift = Lerp(0.18f, 1.05f, PseudoRandom01(index, 1.46f));
        cue.verticalDrift = Lerp(0.10f, 0.62f, PseudoRandom01(index, 1.78f));
        cue.driftSpeed = Lerp(0.014f, 0.042f, PseudoRandom01(index, 2.03f));
        cue.phase = PseudoRandom01(index, 2.41f) * kTwoPi;
        cue.baseScale = isStreak ?
            Lerp(0.46f, 0.82f, PseudoRandom01(index, 2.82f)) :
            Lerp(0.09f, 0.26f, PseudoRandom01(index, 2.82f));
        cue.aspectX = isStreak ? 0.16f : 1.0f;
        cue.aspectY = isStreak ? Lerp(4.0f, 7.2f, PseudoRandom01(index, 3.17f)) : 1.0f;
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
    const float feverSpeedRate = feverSpeedEffectRate_;
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
        const float sidePull = 1.0f + (1.0f - depthRate) *
            (0.28f + speedRate * 0.18f + feverSpeedRate * 0.42f);
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
            Lerp(
                0.66f,
                1.68f + speedRate * 0.28f + feverSpeedRate * 0.70f,
                nearRate) *
            twinkle;
        Math::Vector4 color = cue.color;
        color.w *= Lerp(
            0.44f,
            1.22f + speedRate * 0.22f + feverSpeedRate * 0.58f,
            nearRate) * twinkle;

        Math::Vector3 rotate = cameraRotate;
        rotate.z += cue.phase * 0.34f + railDistance_ * cue.spinSpeed * frameStep;

        cue.object->SetTranslate(position);
        cue.object->SetRotate(rotate);
        cue.object->SetScale({
            scale * cue.aspectX,
            scale * cue.aspectY *
                (1.0f + speedRate * nearRate * 0.36f +
                    feverSpeedRate * nearRate * 1.20f),
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
        { 0.34f, 0.34f, 0.68f },
        0.48f,
        1,
        effectEnemyBulletCoreModel_ ? effectEnemyBulletCoreModel_ : effectBulletGlowModel_,
        { 1.0f, 0.12f, 0.50f, 0.80f },
        { 0.86f, 0.86f, 1.0f },
        nullptr,
        { 1.0f, 0.10f, 0.44f, 0.44f },
        { 0.28f, 2.55f, 1.0f },
        1.50f,
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
    chargeTimer_ = feverTimer_ > 0 ? kChargeShotMax : 0;
    if (isCharged) {
        AddMuzzleFlashEffect(spawnPosition, true);
        chargeFlashTimer_ = 18;
    } else {
        AddMuzzleFlashEffect(spawnPosition, false);
    }
}

void GameRuntime::FireEnemyBullet(
    const Math::Vector3& position,
    EnemyBulletStyle style,
    Math::Vector2 aimOffset)
{
    if (!bulletModel_) {
        return;
    }

    Math::Vector3 spawnPosition = position;
    spawnPosition.z -= 1.0f;
    Math::Vector3 bulletDirection{ 0.0f, 0.0f, -1.0f };
    if (player_) {
        Math::Vector3 targetPosition = player_->GetTranslate();
        targetPosition.x += aimOffset.x;
        targetPosition.y += 0.08f + aimOffset.y;
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

    Math::Vector4 bodyColor{ 1.0f, 0.76f, 0.90f, 1.0f };
    Math::Vector3 bodyScale{ 0.34f, 0.34f, 0.68f };
    Math::Vector4 glowColor{ 1.0f, 0.12f, 0.50f, 0.80f };
    Math::Vector3 glowScale{ 0.86f, 0.86f, 1.0f };
    Math::Vector4 trailColor{ 1.0f, 0.10f, 0.44f, 0.44f };
    Math::Vector3 trailScale{ 0.28f, 2.55f, 1.0f };
    float trailOffset = 1.50f;
    float speedScale = 1.0f;
    float collisionRadius = 0.48f;
    int lifeTimer = 260;
    int damage = 8;

    switch (style) {
    case EnemyBulletStyle::Crossfire:
        bodyColor = { 0.82f, 0.54f, 1.0f, 1.0f };
        bodyScale = { 0.24f, 0.24f, 1.15f };
        glowColor = { 0.72f, 0.18f, 1.0f, 0.92f };
        glowScale = { 0.72f, 1.05f, 1.0f };
        trailColor = { 0.54f, 0.12f, 1.0f, 0.62f };
        trailScale = { 0.20f, 3.30f, 1.0f };
        trailOffset = 1.90f;
        speedScale = 1.12f;
        collisionRadius = 0.42f;
        damage = 7;
        break;
    case EnemyBulletStyle::Sniper:
        bodyColor = { 1.45f, 0.88f, 0.78f, 1.0f };
        bodyScale = { 0.34f, 0.34f, 1.82f };
        glowColor = { 1.0f, 0.08f, 0.18f, 1.0f };
        glowScale = { 1.18f, 1.48f, 1.0f };
        trailColor = { 1.0f, 0.04f, 0.12f, 0.82f };
        trailScale = { 0.24f, 6.80f, 1.0f };
        trailOffset = 3.55f;
        speedScale = 1.67f;
        collisionRadius = 0.56f;
        lifeTimer = 210;
        damage = 14;
        break;
    case EnemyBulletStyle::ShieldOrb:
        bodyColor = { 0.48f, 1.20f, 1.45f, 1.0f };
        bodyScale = { 0.62f, 0.62f, 0.62f };
        glowColor = { 0.10f, 0.92f, 1.0f, 0.94f };
        glowScale = { 1.42f, 1.42f, 1.0f };
        trailColor = { 0.08f, 0.76f, 1.0f, 0.48f };
        trailScale = { 0.34f, 1.42f, 1.0f };
        trailOffset = 0.78f;
        speedScale = 0.78f;
        collisionRadius = 0.62f;
        lifeTimer = 300;
        damage = 6;
        break;
    case EnemyBulletStyle::BossCannon:
        bodyColor = { 1.45f, 0.94f, 0.42f, 1.0f };
        bodyScale = { 0.58f, 0.58f, 1.18f };
        glowColor = { 1.0f, 0.18f, 0.08f, 1.0f };
        glowScale = { 1.38f, 1.58f, 1.0f };
        trailColor = { 1.0f, 0.12f, 0.04f, 0.72f };
        trailScale = { 0.42f, 4.35f, 1.0f };
        trailOffset = 2.45f;
        speedScale = 1.08f;
        collisionRadius = 0.72f;
        lifeTimer = 280;
        damage = 12;
        break;
    case EnemyBulletStyle::Standard:
    default:
        break;
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
        bulletDirection * enemyBulletSpeed_ * speedScale,
        bodyColor,
        lifeTimer,
        bodyScale,
        collisionRadius,
        1,
        effectEnemyBulletCoreModel_ ? effectEnemyBulletCoreModel_ : effectBulletGlowModel_,
        glowColor,
        glowScale,
        nullptr,
        trailColor,
        trailScale,
        trailOffset,
        nullptr,
        { 1.0f, 0.30f, 0.70f, 0.0f },
        { 0.14f, 0.14f, 1.0f },
        damage);
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
    if ((behavior == Enemy::Behavior::StrafeShooter &&
        maxHpOverride >= 7 &&
        enemyHeavyModel_) ||
        (behavior == Enemy::Behavior::Shield && enemyHeavyModel_)) {
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
    case Enemy::Behavior::Support:
        return enemyFormationModel_ ? enemyFormationModel_ : enemyModel_;
    case Enemy::Behavior::Shield:
        return enemyHeavyModel_ ? enemyHeavyModel_ :
            (enemyFormationModel_ ? enemyFormationModel_ : enemyModel_);
    case Enemy::Behavior::Sniper:
        return enemyShooterModel_ ? enemyShooterModel_ : enemyModel_;
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
    case Enemy::Behavior::Support:
        return enemyFormationModel_ ? kEnemyFormationTexturePath : nullptr;
    case Enemy::Behavior::Shield:
        return enemyHeavyModel_ ? kEnemyHeavyTexturePath :
            (enemyFormationModel_ ? kEnemyFormationTexturePath : nullptr);
    case Enemy::Behavior::Sniper:
        return enemyShooterModel_ ? kEnemyShooterTexturePath : nullptr;
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
        stageProgress_ < kBossSpawnDistance ||
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
            stageProgress_ < event.distance) {
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

    if (stageProgress_ < kStageClearDistance) {
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
        { 0.72f, 0.94f, 1.0f, 0.76f }, 0.62f, 4.10f, 0.0f, 0.0f, 1.34f, 0.68f, {});
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
    const bool isHeavyImpact = strength >= 1.35f;
    effect.duration = isHeavyImpact ? 28 : 22;
    effect.strength = strength * (isHeavyImpact ? 1.22f : 1.14f);
    effect.type = HitEffectType::EnemyImpact;

    Model* fireballModel = effectExplosionFireballModel_ ? effectExplosionFireballModel_ : effectImpactBurstModel_;
    Model* sparksModel = effectExplosionSparksModel_ ? effectExplosionSparksModel_ : effectSparkStarModel_;
    Model* debrisModel = effectMagicShardModel_ ? effectMagicShardModel_ : effectSparkStarModel_;

    AddHitEffectVisual(effect, effectGlowRingModel_ ? effectGlowRingModel_ : effectGlowCoreModel_, worldPosition,
        isHeavyImpact ?
            Math::Vector4{ 0.72f, 0.96f, 1.0f, 0.92f } :
            Math::Vector4{ 0.82f, 0.98f, 1.0f, 0.78f },
        isHeavyImpact ? 0.44f : 0.34f,
        isHeavyImpact ? 3.20f : 2.34f,
        0.0f, 0.0f, 1.26f, 0.72f, {});
    AddHitEffectVisual(effect, effectSparkStarModel_ ? effectSparkStarModel_ : effectGlowCoreModel_, worldPosition,
        { 1.0f, 1.0f, 0.96f, 0.96f },
        isHeavyImpact ? 0.54f : 0.40f,
        isHeavyImpact ? 0.46f : 0.34f,
        isHeavyImpact ? 1.20f : 0.82f,
        0.0f, 1.0f, 1.0f, {});
    AddHitEffectVisual(effect, fireballModel, worldPosition,
        { 1.0f, 0.52f, 0.16f, 0.82f }, 0.50f, 0.52f, 0.24f, 0.01f, 1.10f, 0.82f, {});
    AddHitEffectVisual(effect, effectGlowCoreModel_, worldPosition,
        { 0.66f, 0.94f, 1.0f, 0.66f }, 0.34f, 0.48f, -0.18f, 0.0f, 1.42f, 0.54f, {});
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

void GameRuntime::AddEnemyShotTelegraphEffect(
    const Math::Vector3& worldPosition,
    bool isDangerous)
{
    HitEffect effect{};
    effect.worldPosition = worldPosition;
    effect.duration = kEnemyVolleyTelegraphLeadFrames;
    effect.strength = isDangerous ? 1.18f : 0.92f;
    effect.type = HitEffectType::EnemyImpact;

    AddHitEffectVisual(
        effect,
        effectGlowRingModel_ ? effectGlowRingModel_ : effectGlowCoreModel_,
        worldPosition,
        isDangerous ?
            Math::Vector4{ 1.0f, 0.28f, 0.12f, 0.82f } :
            Math::Vector4{ 1.0f, 0.18f, 0.54f, 0.68f },
        isDangerous ? 1.05f : 0.78f,
        -0.68f,
        isDangerous ? 1.10f : -0.82f,
        0.0f,
        1.0f,
        1.0f,
        {});
    AddHitEffectVisual(
        effect,
        effectGlowCoreModel_,
        worldPosition,
        isDangerous ?
            Math::Vector4{ 1.0f, 0.72f, 0.42f, 0.52f } :
            Math::Vector4{ 1.0f, 0.56f, 0.78f, 0.44f },
        isDangerous ? 0.30f : 0.22f,
        0.48f,
        0.0f,
        0.0f,
        1.0f,
        1.0f,
        {});
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
    effect.duration = 34;
    effect.strength = 1.08f;
    effect.type = HitEffectType::PlayerDamage;

    AddHitEffectVisual(effect, effectGlowRingModel_ ? effectGlowRingModel_ : effectGlowCoreModel_, worldPosition,
        { 0.54f, 0.94f, 1.0f, 0.82f }, 0.72f, 2.60f, 0.0f, 0.0f, 1.28f, 0.72f, {});
    AddHitEffectVisual(effect, effectGlowCoreModel_, worldPosition,
        { 1.0f, 0.76f, 0.66f, 0.82f }, 0.82f, 0.42f, 0.0f, 0.0f, 1.20f, 0.76f, { 0.0f, 0.02f, -0.02f });
    AddHitEffectVisual(effect, effectSparkStarModel_, worldPosition,
        { 1.0f, 0.36f, 0.22f, 0.76f }, 0.52f, 0.78f, -1.35f, 0.02f, 1.34f, 0.58f, { -0.078f, 0.048f, 0.012f });
    AddHitEffectVisual(effect, effectSparkStarModel_, worldPosition,
        { 1.0f, 0.72f, 0.34f, 0.68f }, 0.42f, 0.72f, 1.48f, 0.04f, 0.62f, 1.26f, { 0.086f, 0.026f, 0.010f });
    AddHitEffectVisual(effect, effectSparkStarModel_, worldPosition,
        { 0.72f, 0.94f, 1.0f, 0.60f }, 0.34f, 0.62f, -0.92f, 0.06f, 1.22f, 0.52f, { 0.012f, -0.072f, 0.008f });
    if (effect.visualCount > 0) {
        hitEffects_.push_back(std::move(effect));
    }
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

    AddScore(kJustDodgeScoreBonus);
    AddFeverGauge(22);
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
    const int flashDuration =
        isBossHit ?
            (isDestroyed ? 28 : 15) :
            (isDestroyed ?
                (isCharged ? 13 : 7) :
                (isCharged ? kPlayerImpactFlashDuration : 4));
    const int slowDuration =
        isBossHit ?
            (isDestroyed ? 24 : 11) :
            (isDestroyed ?
                (isCharged ? 10 : 5) :
                (isCharged ? kPlayerImpactSlowDuration : 2));
    const float slowScale =
        isBossHit ?
            (isDestroyed ? 0.18f : 0.32f) :
            (isDestroyed ?
                (isCharged ? kPlayerImpactRailSlowScale : 0.62f) :
                (isCharged ? kPlayerImpactRailSlowScale : 0.78f));

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
        isBossHit ? (isDestroyed ? 0.32f : 0.12f) :
            (isDestroyed ? (isCharged ? 0.070f : 0.045f) :
                (isCharged ? 0.052f : 0.018f)),
        isBossHit ? (isDestroyed ? 42 : 14) :
            (isDestroyed ? (isCharged ? 9 : 6) :
                (isCharged ? 7 : 3)));
}

void GameRuntime::TriggerHitConfirm(
    const Math::Vector3& worldPosition,
    bool isCharged,
    bool isBossHit,
    bool isDestroyed)
{
    Math::Vector2 screenPosition{};
    hitConfirmScreen_ = TryProjectToScreen(worldPosition, screenPosition) ?
        screenPosition : reticleScreen_;
    hitConfirmDuration_ = isDestroyed ? (isBossHit ? 30 : 24) :
        (isCharged || isBossHit ? 20 : 16);
    hitConfirmTimer_ = hitConfirmDuration_;
    hitConfirmStrength_ = isBossHit ? 1.35f :
        (isDestroyed ? 1.22f : (isCharged ? 1.14f : 1.0f));
    hitConfirmCharged_ = isCharged;
    hitConfirmBoss_ = isBossHit;
    hitConfirmDestroyed_ = isDestroyed;

    hitConfirmComboCount_ = hitConfirmComboTimer_ > 0 ?
        (std::min)(hitConfirmComboCount_ + 1, 99) : 1;
    hitConfirmComboTimer_ = 90;
}

void GameRuntime::TriggerPlayerDamageFeedback(
    const Math::Vector3& worldPosition,
    const Math::Vector3& incomingVelocity)
{
    Math::Vector2 screenPosition{};
    playerDamageScreen_ = TryProjectToScreen(worldPosition, screenPosition) ?
        screenPosition : reticleScreen_;

    Math::Vector2 sourceDirection{
        -incomingVelocity.x,
        incomingVelocity.y
    };
    const float directionLength = std::sqrt(
        sourceDirection.x * sourceDirection.x +
        sourceDirection.y * sourceDirection.y);
    if (directionLength > 0.001f) {
        sourceDirection.x /= directionLength;
        sourceDirection.y /= directionLength;
    } else {
        sourceDirection = { 0.0f, -1.0f };
    }
    playerDamageDirection_ = sourceDirection;
    playerDamageHudDuration_ = 30;
    playerDamageHudTimer_ = playerDamageHudDuration_;
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
            const float feverRate =
                std::clamp(feverSpeedEffectRate_, 0.0f, 1.0f);
            const float speed =
                (isCore ? 6.6f : 4.8f) + playerExhaustThrust_ * 2.1f +
                feverRate * (isCore ? 3.4f : 2.8f);

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
                Math::Vector4{
                    Lerp(0.78f, 1.0f, feverRate),
                    Lerp(0.97f, 1.0f, feverRate),
                    Lerp(1.0f, 0.92f, feverRate),
                    Lerp(0.92f, 1.0f, feverRate) } :
                Math::Vector4{
                    Lerp(0.06f, 0.34f, feverRate),
                    Lerp(0.48f, 0.82f, feverRate),
                    1.0f,
                    Lerp(0.66f, 0.86f, feverRate) };
            particle.age = 0.0f;
            particle.lifetime =
                (isCore ? 0.15f : 0.28f) +
                feverRate * (isCore ? 0.06f : 0.09f);
            particle.startSize =
                (isCore ? 0.16f : 0.27f) *
                (0.92f + playerExhaustThrust_ * 0.22f) *
                (1.0f + feverRate * (isCore ? 0.38f : 0.52f));
            particle.endSize =
                (isCore ? 0.025f : 0.075f) * (1.0f + feverRate * 0.40f);
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
    const float feverThrustBoost = feverSpeedEffectRate_;
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
            justDodgeBoost * 0.34f +
            feverThrustBoost * 0.95f,
        0.0f,
        1.75f);
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
        if (isOuterFlame) {
            color.x = Lerp(color.x, 0.46f, feverThrustBoost);
            color.y = Lerp(color.y, 0.90f, feverThrustBoost);
            color.z = Lerp(color.z, 1.0f, feverThrustBoost);
        } else {
            color.x = Lerp(color.x, 1.0f, feverThrustBoost);
            color.y = Lerp(color.y, 1.0f, feverThrustBoost);
            color.z = Lerp(color.z, 0.92f, feverThrustBoost);
        }
        color.w *= flicker *
            (0.76f + speedRate * 0.10f + playerExhaustThrust_ * alphaResponse) *
            (1.0f + feverThrustBoost * 0.20f);
        color.w = std::clamp(color.w, 0.0f, 1.0f);

        const float feverWidthScale =
            1.0f + feverThrustBoost *
                (isOuterFlame ? 0.24f : isInnerFlame ? 0.18f : 0.12f);
        const float feverLengthScale =
            1.0f + feverThrustBoost *
                (isOuterFlame ? 0.56f : isInnerFlame ? 0.42f : 0.18f);

        aura.object->SetModel(aura.model);
        aura.object->SetTranslate(translate);
        aura.object->SetRotate(rotate);
        aura.object->SetScale({
            aura.baseSize * aura.aspectX * pulse *
                (1.0f + playerExhaustThrust_ * widthResponse) * feverWidthScale,
            aura.baseSize * aura.aspectY * (2.0f - pulse) *
                (1.0f + playerExhaustThrust_ * lengthResponse) * feverLengthScale,
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

#ifdef ENABLE_DEBUG_GUI
        if (ImGui::CollapsingHeader("デバッグ操作", ImGuiTreeNodeFlags_DefaultOpen)) {
            const bool isFeverActive = feverTimer_ > 0;
            const float feverRate = isFeverActive ?
                static_cast<float>(feverTimer_) / static_cast<float>(kFeverDurationFrames) :
                static_cast<float>(feverGauge_) / static_cast<float>(kFeverGaugeMax);
            char feverLabel[64]{};
            if (isFeverActive) {
                std::snprintf(
                    feverLabel,
                    sizeof(feverLabel),
                    "FEVER %.1f sec",
                    static_cast<double>(feverTimer_) / 60.0);
            } else {
                std::snprintf(
                    feverLabel,
                    sizeof(feverLabel),
                    "Gauge %d / %d",
                    feverGauge_,
                    kFeverGaugeMax);
            }
            ImGui::ProgressBar(
                (std::clamp)(feverRate, 0.0f, 1.0f),
                ImVec2(-1.0f, 0.0f),
                feverLabel);

            const float buttonSpacing = ImGui::GetStyle().ItemSpacing.x;
            const float halfButtonWidth =
                (ImGui::GetContentRegionAvail().x - buttonSpacing) * 0.5f;
            if (ImGui::Button("ゲージ MAX", ImVec2(halfButtonWidth, 0.0f))) {
                feverGauge_ = kFeverGaugeMax;
                editorStatusMessage_ = "フィーバーゲージを最大にしました。";
            }
            ImGui::SameLine();
            if (ImGui::Button("即時発動", ImVec2(halfButtonWidth, 0.0f))) {
                ActivateFever();
                editorStatusMessage_ = "フィーバーを発動しました。";
            }
            ImGui::BeginDisabled(!isFeverActive);
            if (ImGui::Button("終了", ImVec2(-1.0f, 0.0f))) {
                feverTimer_ = 0;
                feverActivationFlashTimer_ = 0;
                editorStatusMessage_ = "フィーバーを終了しました。";
            }
            ImGui::EndDisabled();

            ImGui::SeparatorText("フェーズ移動");
            const bool phaseJumpDisabled = isGameOver_ || isGameClear_;
            ImGui::BeginDisabled(phaseJumpDisabled);
            if (ImGui::Button("Wave 1", ImVec2(halfButtonWidth, 0.0f))) {
                DebugJumpToStagePhase(0);
            }
            ImGui::SameLine();
            if (ImGui::Button("Wave 2", ImVec2(halfButtonWidth, 0.0f))) {
                DebugJumpToStagePhase(1);
            }
            if (ImGui::Button("Wave 3", ImVec2(halfButtonWidth, 0.0f))) {
                DebugJumpToStagePhase(2);
            }
            ImGui::SameLine();
            if (ImGui::Button("Boss", ImVec2(halfButtonWidth, 0.0f))) {
                DebugJumpToStagePhase(3);
            }
            ImGui::EndDisabled();
            if (phaseJumpDisabled) {
                ImGui::TextDisabled("フェーズ移動はゲーム進行中のみ使用できます。 ");
            }
        }
#endif

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
void GameRuntime::DrawFeverBackdrop()
{
    const float feverVisualRate =
        std::clamp(feverSpeedEffectRate_, 0.0f, 1.0f);
    if (feverVisualRate <= 0.001f) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    Math::Vector2 hudMin{};
    Math::Vector2 hudSize{};
    GetEffectiveHudViewportRect(hudMin, hudSize);
    const ImVec2 origin(hudMin.x, hudMin.y);
    const ImVec2 drawSize(hudSize.x, hudSize.y);
    const ImVec2 viewportMax(origin.x + drawSize.x, origin.y + drawSize.y);
    if (drawSize.x <= 1.0f || drawSize.y <= 1.0f) {
        return;
    }

    const float rainbowTime = cameraTimer_ * 0.008f;
    const auto rainbowColor = [rainbowTime, feverVisualRate](float offset, int alpha) {
        float hue = std::fmod(rainbowTime + offset, 1.0f);
        if (hue < 0.0f) {
            hue += 1.0f;
        }
        float red = 1.0f;
        float green = 1.0f;
        float blue = 1.0f;
        ImGui::ColorConvertHSVtoRGB(hue, 0.76f, 1.0f, red, green, blue);
        return IM_COL32(
            static_cast<int>(red * 255.0f),
            static_cast<int>(green * 255.0f),
            static_cast<int>(blue * 255.0f),
            (std::clamp)(
                static_cast<int>(static_cast<float>(alpha) * feverVisualRate),
                0,
                255));
    };

    const float ambiencePulse =
        0.5f + 0.5f * std::sin(cameraTimer_ * 0.055f);
    const int dimAlpha = static_cast<int>(
        (50.0f + ambiencePulse * 9.0f) * feverVisualRate);
    drawList->AddRectFilled(
        origin,
        viewportMax,
        IM_COL32(2, 4, 15, dimAlpha));
    drawList->AddRectFilled(
        origin,
        viewportMax,
        rainbowColor(0.0f, 8));

    const float vignetteSize =
        std::clamp((std::min)(drawSize.x, drawSize.y) * 0.14f, 54.0f, 112.0f);
    const int vignetteAlpha = static_cast<int>(
        (68.0f + ambiencePulse * 18.0f) * feverVisualRate);
    drawList->AddRectFilledMultiColor(
        origin,
        ImVec2(viewportMax.x, origin.y + vignetteSize),
        IM_COL32(0, 1, 8, vignetteAlpha),
        IM_COL32(0, 1, 8, vignetteAlpha),
        IM_COL32(0, 1, 8, 0),
        IM_COL32(0, 1, 8, 0));
    drawList->AddRectFilledMultiColor(
        ImVec2(origin.x, viewportMax.y - vignetteSize),
        viewportMax,
        IM_COL32(0, 1, 8, 0),
        IM_COL32(0, 1, 8, 0),
        IM_COL32(0, 1, 8, vignetteAlpha),
        IM_COL32(0, 1, 8, vignetteAlpha));
    drawList->AddRectFilledMultiColor(
        origin,
        ImVec2(origin.x + vignetteSize, viewportMax.y),
        IM_COL32(0, 1, 8, vignetteAlpha),
        IM_COL32(0, 1, 8, 0),
        IM_COL32(0, 1, 8, 0),
        IM_COL32(0, 1, 8, vignetteAlpha));
    drawList->AddRectFilledMultiColor(
        ImVec2(viewportMax.x - vignetteSize, origin.y),
        viewportMax,
        IM_COL32(0, 1, 8, 0),
        IM_COL32(0, 1, 8, vignetteAlpha),
        IM_COL32(0, 1, 8, vignetteAlpha),
        IM_COL32(0, 1, 8, 0));

    const float activationElapsedFrames =
        static_cast<float>(kFeverActivationFlashFrames - feverActivationFlashTimer_);
    const float accelerationKickPhase =
        std::clamp(activationElapsedFrames / 28.0f, 0.0f, 1.0f);
    const float feverAccelerationKick =
        feverActivationFlashTimer_ > 0 ?
        std::sin(accelerationKickPhase * std::numbers::pi_v<float>) : 0.0f;
    const float windIntensity = std::clamp(
        feverVisualRate + feverAccelerationKick * 0.48f,
        0.0f,
        1.28f);
    const ImVec2 windVanishingPoint(
        origin.x + drawSize.x * 0.50f,
        origin.y + drawSize.y * 0.445f);
    constexpr int kWindStreakCount = 60;
    constexpr float kPi = 3.14159265358979323846f;
    drawList->PushClipRect(origin, viewportMax, true);
    for (int index = 0; index < kWindStreakCount; ++index) {
        float seed = std::sin(
            static_cast<float>(index) * 12.9898f + 78.233f) * 43758.5453f;
        seed -= std::floor(seed);
        const float angle =
            2.0f * kPi *
            (static_cast<float>(index) + seed * 0.58f) /
            static_cast<float>(kWindStreakCount);
        const float travelSpeed =
            0.028f + static_cast<float>(index % 5) * 0.0017f +
            feverAccelerationKick * 0.009f;
        float phase = std::fmod(cameraTimer_ * travelSpeed + seed, 1.0f);
        if (phase < 0.0f) {
            phase += 1.0f;
        }

        const float frontRate = 0.17f + phase * 1.04f;
        const float edgeRate =
            std::clamp((frontRate - 0.24f) / 0.80f, 0.0f, 1.0f);
        const float streakLength =
            0.035f + edgeRate * (0.16f + seed * 0.08f);
        const float backRate = (std::max)(0.15f, frontRate - streakLength);
        const float directionX = std::cos(angle) * drawSize.x * 0.72f;
        const float directionY = std::sin(angle) * drawSize.y * 0.72f;
        const ImVec2 streakStart(
            windVanishingPoint.x + directionX * backRate,
            windVanishingPoint.y + directionY * backRate);
        const ImVec2 streakEnd(
            windVanishingPoint.x + directionX * frontRate,
            windVanishingPoint.y + directionY * frontRate);
        const float lifeFade = std::sin(phase * kPi);
        const int coreAlpha = static_cast<int>(
            (18.0f + edgeRate * 108.0f) * lifeFade * windIntensity);
        if (coreAlpha <= 1) {
            continue;
        }

        const bool useRainbowGlint = index % 6 == 0;
        const ImU32 coreColor = useRainbowGlint ?
            rainbowColor(seed + phase * 0.14f, coreAlpha) :
            IM_COL32(190, 236, 255, coreAlpha);
        drawList->AddLine(
            streakStart,
            streakEnd,
            IM_COL32(90, 190, 255, coreAlpha / 4),
            4.0f + edgeRate * 5.0f);
        drawList->AddLine(
            streakStart,
            streakEnd,
            coreColor,
            1.0f + edgeRate * 1.9f);
        drawList->AddCircleFilled(
            streakEnd,
            0.8f + edgeRate * 1.5f,
            IM_COL32(
                235,
                252,
                255,
                (std::min)(
                    coreAlpha + static_cast<int>(38.0f * feverVisualRate),
                    190)),
            8);
    }
    drawList->PopClipRect();

    constexpr int kBackdropHorizontalSegments = 20;
    constexpr int kBackdropVerticalSegments = 12;
    for (int layer = 0; layer < 4; ++layer) {
        const float margin = 6.0f + static_cast<float>(layer) * 4.0f;
        const float left = origin.x + margin;
        const float right = viewportMax.x - margin;
        const float top = origin.y + margin;
        const float bottom = viewportMax.y - margin;
        const int alpha = 34 - layer * 6;
        const float thickness = 7.0f - static_cast<float>(layer) * 1.2f;
        for (int segment = 0; segment < kBackdropHorizontalSegments; ++segment) {
            const float rate0 =
                static_cast<float>(segment) /
                static_cast<float>(kBackdropHorizontalSegments);
            const float rate1 =
                static_cast<float>(segment + 1) /
                static_cast<float>(kBackdropHorizontalSegments);
            drawList->AddLine(
                ImVec2(Lerp(left, right, rate0), top),
                ImVec2(Lerp(left, right, rate1) + 1.0f, top),
                rainbowColor(rate0 * 0.78f + static_cast<float>(layer) * 0.05f, alpha),
                thickness);
            drawList->AddLine(
                ImVec2(Lerp(left, right, rate0), bottom),
                ImVec2(Lerp(left, right, rate1) + 1.0f, bottom),
                rainbowColor(0.78f - rate0 * 0.78f + static_cast<float>(layer) * 0.05f, alpha),
                thickness);
        }
        for (int segment = 0; segment < kBackdropVerticalSegments; ++segment) {
            const float rate0 =
                static_cast<float>(segment) /
                static_cast<float>(kBackdropVerticalSegments);
            const float rate1 =
                static_cast<float>(segment + 1) /
                static_cast<float>(kBackdropVerticalSegments);
            drawList->AddLine(
                ImVec2(left, Lerp(top, bottom, rate0)),
                ImVec2(left, Lerp(top, bottom, rate1) + 1.0f),
                rainbowColor(0.18f + rate0 * 0.46f + static_cast<float>(layer) * 0.05f, alpha),
                thickness);
            drawList->AddLine(
                ImVec2(right, Lerp(top, bottom, rate0)),
                ImVec2(right, Lerp(top, bottom, rate1) + 1.0f),
                rainbowColor(0.94f - rate0 * 0.46f + static_cast<float>(layer) * 0.05f, alpha),
                thickness);
        }
    }

    const auto getPerimeterPoint = [origin, viewportMax](float rate, float margin) {
        const float left = origin.x + margin;
        const float right = viewportMax.x - margin;
        const float top = origin.y + margin;
        const float bottom = viewportMax.y - margin;
        const float width = (std::max)(right - left, 1.0f);
        const float height = (std::max)(bottom - top, 1.0f);
        const float perimeter = (width + height) * 2.0f;
        float distance = std::fmod(rate, 1.0f) * perimeter;
        if (distance < 0.0f) {
            distance += perimeter;
        }
        if (distance < width) {
            return ImVec2(left + distance, top);
        }
        distance -= width;
        if (distance < height) {
            return ImVec2(right, top + distance);
        }
        distance -= height;
        if (distance < width) {
            return ImVec2(right - distance, bottom);
        }
        distance -= width;
        return ImVec2(left, bottom - distance);
    };

    constexpr int kBorderSparkleCount = 42;
    for (int index = 0; index < kBorderSparkleCount; ++index) {
        const float indexRate =
            static_cast<float>(index) / static_cast<float>(kBorderSparkleCount);
        const float travelRate =
            indexRate + cameraTimer_ * (0.00042f + 0.00005f * static_cast<float>(index % 3));
        const ImVec2 position = getPerimeterPoint(
            travelRate,
            17.0f + static_cast<float>(index % 3) * 4.0f);
        const float rawTwinkle =
            0.5f + 0.5f * std::sin(cameraTimer_ * 0.18f + static_cast<float>(index) * 1.73f);
        const float twinkle = rawTwinkle * rawTwinkle * rawTwinkle;
        const float radius = 2.2f + twinkle * (5.8f + static_cast<float>(index % 4));
        const int glowAlpha = static_cast<int>(65.0f + twinkle * 125.0f);
        const ImU32 color = rainbowColor(indexRate + twinkle * 0.10f, glowAlpha);
        drawList->AddCircleFilled(position, radius * 1.35f, rainbowColor(indexRate, glowAlpha / 3), 16);
        drawList->AddQuadFilled(
            ImVec2(position.x, position.y - radius * 1.55f),
            ImVec2(position.x + radius * 0.34f, position.y),
            ImVec2(position.x, position.y + radius * 1.55f),
            ImVec2(position.x - radius * 0.34f, position.y),
            color);
        drawList->AddQuadFilled(
            ImVec2(position.x - radius * 1.55f, position.y),
            ImVec2(position.x, position.y - radius * 0.34f),
            ImVec2(position.x + radius * 1.55f, position.y),
            ImVec2(position.x, position.y + radius * 0.34f),
            color);
        drawList->AddCircleFilled(
            position,
            (std::max)(1.0f, radius * 0.24f),
            IM_COL32(
                255,
                255,
                255,
                static_cast<int>((145.0f + twinkle * 110.0f) * feverVisualRate)),
            10);
    }
}

void GameRuntime::DrawEnemyTypeTelegraphs()
{
    if (!player_) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const bool sniperTelegraphActive =
        enemyShotTimer_ > 0 &&
        enemyShotTimer_ <= kSniperTelegraphLeadFrames;
    Math::Vector2 playerScreen{};
    Math::Vector3 playerAimPosition = player_->GetTranslate();
    playerAimPosition.y += 0.12f;
    const bool hasPlayerScreenPosition =
        TryProjectToScreen(playerAimPosition, playerScreen);

    const float chargeRate = 1.0f - std::clamp(
        static_cast<float>(enemyShotTimer_) /
            static_cast<float>(kSniperTelegraphLeadFrames),
        0.0f,
        1.0f);
    const float pulse =
        0.5f + 0.5f * std::sin(cameraTimer_ * 0.48f);
    const int lineAlpha = static_cast<int>(
        70.0f + chargeRate * 130.0f + pulse * 35.0f);

    for (const auto& enemy : enemies_) {
        if (!enemy || enemy->IsDead()) {
            continue;
        }

        Math::Vector2 enemyScreen{};
        if (!TryProjectToScreen(enemy->GetAimPosition(), enemyScreen)) {
            continue;
        }

        if (enemy->IsShield() && enemy->HasShield()) {
            const float shieldRate = std::clamp(
                static_cast<float>(enemy->GetShieldHp()) /
                    static_cast<float>((std::max)(enemy->GetShieldMaxHp(), 1)),
                0.0f,
                1.0f);
            const float shieldPulse =
                0.5f + 0.5f * std::sin(cameraTimer_ * 0.16f + enemyScreen.x * 0.01f);
            const int shieldAlpha = static_cast<int>(
                105.0f + shieldRate * 90.0f + shieldPulse * 35.0f);
            const ImVec2 center(enemyScreen.x, enemyScreen.y);
            const float radius = 25.0f + shieldPulse * 4.0f;
            drawList->AddCircleFilled(
                center,
                radius,
                IM_COL32(32, 176, 255, shieldAlpha / 7),
                32);
            drawList->AddCircle(
                center,
                radius,
                IM_COL32(86, 222, 255, shieldAlpha),
                32,
                2.5f);
            drawList->AddCircle(
                center,
                radius + 5.0f,
                IM_COL32(180, 248, 255, shieldAlpha / 2),
                32,
                1.0f);
            char shieldLabel[32]{};
            std::snprintf(
                shieldLabel,
                sizeof(shieldLabel),
                "SHIELD %d",
                enemy->GetShieldHp());
            drawList->AddText(
                ImVec2(center.x + radius + 7.0f, center.y - 8.0f),
                IM_COL32(136, 236, 255, shieldAlpha),
                shieldLabel);
        }

        if (enemy->IsSupport()) {
            const float supportPulse =
                0.5f + 0.5f * std::sin(cameraTimer_ * 0.18f + enemyScreen.y * 0.01f);
            const int supportAlpha = static_cast<int>(
                120.0f + supportPulse * 95.0f);
            const ImVec2 supportCenter(enemyScreen.x, enemyScreen.y);
            drawList->AddCircleFilled(
                supportCenter,
                13.0f + supportPulse * 3.0f,
                IM_COL32(112, 255, 94, supportAlpha / 6),
                20);
            drawList->AddCircle(
                supportCenter,
                17.0f + supportPulse * 4.0f,
                IM_COL32(132, 255, 104, supportAlpha),
                4,
                2.5f);
            drawList->AddText(
                ImVec2(supportCenter.x + 21.0f, supportCenter.y - 9.0f),
                IM_COL32(174, 255, 144, supportAlpha),
                "SUPPORT BOOST");

            int linkCount = 0;
            constexpr int kMaxSupportLinks = 3;
            for (const auto& linkedEnemy : enemies_) {
                if (!linkedEnemy || linkedEnemy.get() == enemy.get() ||
                    linkedEnemy->IsDead() || linkedEnemy->IsSupport()) {
                    continue;
                }
                Math::Vector2 linkedScreen{};
                if (!TryProjectToScreen(linkedEnemy->GetAimPosition(), linkedScreen)) {
                    continue;
                }
                drawList->AddLine(
                    supportCenter,
                    ImVec2(linkedScreen.x, linkedScreen.y),
                    IM_COL32(102, 255, 132, supportAlpha / 2),
                    1.5f + supportPulse);
                if (++linkCount >= kMaxSupportLinks) {
                    break;
                }
            }
        }

        if (!sniperTelegraphActive || !hasPlayerScreenPosition ||
            !enemy->IsSniper() || !enemy->CanShoot()) {
            continue;
        }

        const ImVec2 start(enemyScreen.x, enemyScreen.y);
        const ImVec2 end(playerScreen.x, playerScreen.y);
        drawList->AddLine(
            start,
            end,
            IM_COL32(20, 0, 10, lineAlpha / 2),
            5.0f);
        drawList->AddLine(
            start,
            end,
            IM_COL32(255, 42, 92, lineAlpha),
            1.5f + chargeRate * 1.5f);
        drawList->AddCircle(
            start,
            15.0f + chargeRate * 7.0f,
            IM_COL32(255, 74, 112, lineAlpha),
            20,
            2.0f);
        drawList->AddCircle(
            end,
            10.0f + pulse * 5.0f,
            IM_COL32(255, 52, 104, lineAlpha),
            16,
            2.0f);
        drawList->AddText(
            ImVec2(start.x + 18.0f, start.y - 22.0f),
            IM_COL32(255, 126, 150, lineAlpha),
            "SNIPER LOCK");
    }
}

void GameRuntime::DrawHud()
{
    if (!player_) {
        return;
    }

    DrawFeverBackdrop();
    DrawEnemyTypeTelegraphs();

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
        if (fillMax.x > min.x + 2.0f) {
            drawList->AddLine(
                ImVec2(min.x + 3.0f, min.y + 2.0f),
                ImVec2(fillMax.x - 2.0f, min.y + 2.0f),
                IM_COL32(255, 255, 255, 62),
                1.0f);
        }
        for (int segment = 1; segment < 4; ++segment) {
            const float x = min.x + (max.x - min.x) *
                (static_cast<float>(segment) / 4.0f);
            drawList->AddLine(
                ImVec2(x, min.y + 2.0f),
                ImVec2(x, max.y - 2.0f),
                IM_COL32(9, 18, 30, 118),
                1.0f);
        }
        drawList->AddRect(min, max, IM_COL32(228, 242, 255, 95), 4.0f);
    };
    auto drawTechCorners = [drawList](
                               const ImVec2& min,
                               const ImVec2& max,
                               ImU32 color) {
        constexpr float kLength = 17.0f;
        constexpr float kOffset = 3.0f;
        constexpr float kThickness = 2.0f;
        drawList->AddLine(
            ImVec2(min.x - kOffset, min.y + kLength),
            ImVec2(min.x - kOffset, min.y - kOffset),
            color,
            kThickness);
        drawList->AddLine(
            ImVec2(min.x - kOffset, min.y - kOffset),
            ImVec2(min.x + kLength, min.y - kOffset),
            color,
            kThickness);
        drawList->AddLine(
            ImVec2(max.x - kLength, min.y - kOffset),
            ImVec2(max.x + kOffset, min.y - kOffset),
            color,
            kThickness);
        drawList->AddLine(
            ImVec2(max.x + kOffset, min.y - kOffset),
            ImVec2(max.x + kOffset, min.y + kLength),
            color,
            kThickness);
        drawList->AddLine(
            ImVec2(min.x - kOffset, max.y - kLength),
            ImVec2(min.x - kOffset, max.y + kOffset),
            color,
            kThickness);
        drawList->AddLine(
            ImVec2(min.x - kOffset, max.y + kOffset),
            ImVec2(min.x + kLength, max.y + kOffset),
            color,
            kThickness);
        drawList->AddLine(
            ImVec2(max.x - kLength, max.y + kOffset),
            ImVec2(max.x + kOffset, max.y + kOffset),
            color,
            kThickness);
        drawList->AddLine(
            ImVec2(max.x + kOffset, max.y + kOffset),
            ImVec2(max.x + kOffset, max.y - kLength),
            color,
            kThickness);
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
    drawList->AddRectFilledMultiColor(
        playerPanelMin,
        ImVec2(playerPanelMax.x, playerPanelMin.y + 4.0f),
        IM_COL32(116, 238, 255, 225),
        IM_COL32(92, 164, 255, 185),
        IM_COL32(92, 164, 255, 185),
        IM_COL32(116, 238, 255, 225));
    drawTechCorners(
        playerPanelMin,
        playerPanelMax,
        IM_COL32(120, 226, 255, 155));
    drawList->AddText(
        ImVec2(playerPanelMin.x + 16.0f, playerPanelMin.y + 14.0f),
        IM_COL32(232, 244, 255, 245),
        "PLAYER // UNIT 01");
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
    drawList->AddRectFilledMultiColor(
        scorePanelMin,
        ImVec2(scorePanelMax.x, scorePanelMin.y + 3.0f),
        IM_COL32(255, 226, 122, 205),
        IM_COL32(255, 158, 92, 155),
        IM_COL32(255, 158, 92, 155),
        IM_COL32(255, 226, 122, 205));
    drawTechCorners(
        scorePanelMin,
        scorePanelMax,
        IM_COL32(255, 220, 126, 135));
    drawList->AddText(
        ImVec2(scorePanelMin.x + 14.0f, scorePanelMin.y + 10.0f),
        IM_COL32(248, 225, 128, 240),
        "MISSION // スコア");
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
    DrawHitConfirmHud();
    DrawPlayerDamageHud();
    DrawFeverHud();
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

void GameRuntime::DrawFeverHud()
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
    const bool isActive = feverTimer_ > 0;
    const bool isReady = !isActive && feverGauge_ >= kFeverGaugeMax;
    const float rate = isActive ?
        static_cast<float>(feverTimer_) / static_cast<float>(kFeverDurationFrames) :
        static_cast<float>(feverGauge_) / static_cast<float>(kFeverGaugeMax);
    const float pulse = 0.5f + 0.5f * std::sin(cameraTimer_ * (isActive ? 8.5f : 5.0f));
    const float rainbowTime = cameraTimer_ * (isActive ? 0.010f : 0.006f);
    const auto rainbowColor = [rainbowTime](float offset, int alpha) {
        float hue = std::fmod(rainbowTime + offset, 1.0f);
        if (hue < 0.0f) {
            hue += 1.0f;
        }
        float red = 1.0f;
        float green = 1.0f;
        float blue = 1.0f;
        ImGui::ColorConvertHSVtoRGB(hue, 0.78f, 1.0f, red, green, blue);
        return IM_COL32(
            static_cast<int>(red * 255.0f),
            static_cast<int>(green * 255.0f),
            static_cast<int>(blue * 255.0f),
            (std::clamp)(alpha, 0, 255));
    };

    const float barWidth = std::clamp(drawSize.x * 0.35f, 340.0f, 520.0f);
    const ImVec2 panelMin(
        origin.x + drawSize.x * 0.5f - barWidth * 0.5f,
        origin.y + drawSize.y - 58.0f);
    const ImVec2 panelMax(panelMin.x + barWidth, panelMin.y + 40.0f);
    const ImVec2 barMin(panelMin.x + 10.0f, panelMin.y + 23.0f);
    const ImVec2 barMax(panelMax.x - 10.0f, panelMax.y - 7.0f);
    const ImVec2 fillMax(
        barMin.x + (barMax.x - barMin.x) * std::clamp(rate, 0.0f, 1.0f),
        barMax.y);
    const ImU32 borderColor = isActive || isReady ?
        rainbowColor(0.0f, isReady ? 245 : 225) :
        IM_COL32(106, 210, 255, 155);

    drawList->AddRectFilled(
        ImVec2(panelMin.x + 3.0f, panelMin.y + 4.0f),
        ImVec2(panelMax.x + 3.0f, panelMax.y + 4.0f),
        IM_COL32(0, 0, 0, 94),
        5.0f);
    drawList->AddRectFilled(panelMin, panelMax, IM_COL32(7, 12, 25, 208), 5.0f);
    drawList->AddRect(panelMin, panelMax, borderColor, 5.0f, 0, isReady ? 2.5f : 1.5f);
    drawList->AddRectFilled(barMin, barMax, IM_COL32(16, 25, 42, 235), 3.0f);
    if (fillMax.x > barMin.x) {
        if (isActive || isReady) {
            constexpr int kRainbowBarSegments = 18;
            const float filledWidth = fillMax.x - barMin.x;
            for (int segment = 0; segment < kRainbowBarSegments; ++segment) {
                const float segmentRate0 =
                    static_cast<float>(segment) / static_cast<float>(kRainbowBarSegments);
                const float segmentRate1 =
                    static_cast<float>(segment + 1) / static_cast<float>(kRainbowBarSegments);
                const ImVec2 segmentMin(
                    barMin.x + filledWidth * segmentRate0,
                    barMin.y);
                const ImVec2 segmentMax(
                    barMin.x + filledWidth * segmentRate1 + 1.0f,
                    barMax.y);
                drawList->AddRectFilledMultiColor(
                    segmentMin,
                    segmentMax,
                    rainbowColor(segmentRate0 * 0.82f, 245),
                    rainbowColor(segmentRate1 * 0.82f, 245),
                    rainbowColor(segmentRate1 * 0.82f + 0.05f, 225),
                    rainbowColor(segmentRate0 * 0.82f + 0.05f, 225));
            }
            const float sweepRate = std::fmod(cameraTimer_ * 0.022f, 1.0f);
            const float sweepX = barMin.x + filledWidth * sweepRate;
            const float sweepHalfWidth = 7.0f + pulse * 4.0f;
            drawList->AddRectFilledMultiColor(
                ImVec2((std::max)(barMin.x, sweepX - sweepHalfWidth), barMin.y + 1.0f),
                ImVec2((std::min)(fillMax.x, sweepX + sweepHalfWidth), barMax.y - 1.0f),
                IM_COL32(255, 255, 255, 15),
                IM_COL32(255, 255, 255, 180),
                IM_COL32(255, 255, 255, 115),
                IM_COL32(255, 255, 255, 10));
        } else {
            drawList->AddRectFilledMultiColor(
                barMin,
                fillMax,
                IM_COL32(72, 220, 255, 245),
                IM_COL32(255, 82, 218, 245),
                IM_COL32(255, 130, 112, 235),
                IM_COL32(96, 190, 255, 235));
        }
        drawList->AddLine(
            ImVec2(barMin.x + 2.0f, barMin.y + 2.0f),
            ImVec2(fillMax.x - 2.0f, barMin.y + 2.0f),
            IM_COL32(255, 255, 255, 150),
            1.0f);
    }
    drawList->AddRect(barMin, barMax, IM_COL32(212, 238, 255, 110), 3.0f);
    for (int segment = 1; segment < 10; ++segment) {
        const float x = barMin.x + (barMax.x - barMin.x) *
            (static_cast<float>(segment) / 10.0f);
        drawList->AddLine(
            ImVec2(x, barMin.y + 1.0f),
            ImVec2(x, barMax.y - 1.0f),
            IM_COL32(7, 15, 28, 125),
            1.0f);
    }

    const char* status = isActive ? "JACKPOT // SCORE x3" :
        isReady ? "E // JACKPOT" : "HIT  DESTROY  JUST DODGE";
    drawList->AddText(
        ImVec2(panelMin.x + 10.0f, panelMin.y + 5.0f),
        isActive || isReady ? rainbowColor(0.12f, 255) : IM_COL32(152, 230, 255, 235),
        "FEVER");
    const ImVec2 statusSize = ImGui::CalcTextSize(status);
    drawList->AddText(
        ImVec2(panelMax.x - statusSize.x - 10.0f, panelMin.y + 5.0f),
        isReady ?
            IM_COL32(255, 232, 122, static_cast<int>(205.0f + pulse * 50.0f)) :
            IM_COL32(218, 230, 244, 220),
        status);

    if (isReady) {
        drawList->AddRect(
            ImVec2(panelMin.x - 4.0f, panelMin.y - 4.0f),
            ImVec2(panelMax.x + 4.0f, panelMax.y + 4.0f),
            rainbowColor(0.42f, static_cast<int>(65.0f + pulse * 105.0f)),
            7.0f,
            0,
            3.0f);
    }

    if (isActive) {
        const int edgeAlpha = static_cast<int>(45.0f + pulse * 38.0f);
        constexpr int kHorizontalEdgeSegments = 16;
        constexpr int kVerticalEdgeSegments = 9;
        const float edgeLeft = origin.x + 7.0f;
        const float edgeRight = origin.x + drawSize.x - 7.0f;
        const float edgeTop = origin.y + 7.0f;
        const float edgeBottom = origin.y + drawSize.y - 7.0f;
        for (int segment = 0; segment < kHorizontalEdgeSegments; ++segment) {
            const float rate0 =
                static_cast<float>(segment) / static_cast<float>(kHorizontalEdgeSegments);
            const float rate1 =
                static_cast<float>(segment + 1) / static_cast<float>(kHorizontalEdgeSegments);
            const float x0 = Lerp(edgeLeft, edgeRight, rate0);
            const float x1 = Lerp(edgeLeft, edgeRight, rate1);
            drawList->AddLine(
                ImVec2(x0, edgeTop),
                ImVec2(x1 + 1.0f, edgeTop),
                rainbowColor(rate0 * 0.76f, edgeAlpha),
                4.0f);
            drawList->AddLine(
                ImVec2(x0, edgeBottom),
                ImVec2(x1 + 1.0f, edgeBottom),
                rainbowColor(0.76f - rate0 * 0.76f, edgeAlpha),
                4.0f);
        }
        for (int segment = 0; segment < kVerticalEdgeSegments; ++segment) {
            const float rate0 =
                static_cast<float>(segment) / static_cast<float>(kVerticalEdgeSegments);
            const float rate1 =
                static_cast<float>(segment + 1) / static_cast<float>(kVerticalEdgeSegments);
            const float y0 = Lerp(edgeTop, edgeBottom, rate0);
            const float y1 = Lerp(edgeTop, edgeBottom, rate1);
            drawList->AddLine(
                ImVec2(edgeLeft, y0),
                ImVec2(edgeLeft, y1 + 1.0f),
                rainbowColor(0.16f + rate0 * 0.48f, edgeAlpha),
                4.0f);
            drawList->AddLine(
                ImVec2(edgeRight, y0),
                ImVec2(edgeRight, y1 + 1.0f),
                rainbowColor(0.92f - rate0 * 0.48f, edgeAlpha),
                4.0f);
        }
        drawList->AddRect(
            ImVec2(origin.x + 12.0f, origin.y + 12.0f),
            ImVec2(origin.x + drawSize.x - 12.0f, origin.y + drawSize.y - 12.0f),
            IM_COL32(255, 255, 255, edgeAlpha / 3),
            0.0f,
            0,
            1.5f);
    }

    if (feverActivationFlashTimer_ > 0) {
        const float flashRate =
            static_cast<float>(feverActivationFlashTimer_) /
            static_cast<float>(kFeverActivationFlashFrames);
        const float elapsed = 1.0f - flashRate;
        const float bannerAlpha = std::clamp(flashRate * 2.5f, 0.0f, 1.0f);
        const char* banner = "FEVER TIME";
        const float fontSize = 34.0f + std::sin(elapsed * std::numbers::pi_v<float>) * 9.0f;
        const ImVec2 textSize =
            ImGui::GetFont()->CalcTextSizeA(fontSize, 100000.0f, 0.0f, banner);
        const ImVec2 bannerPosition(
            origin.x + drawSize.x * 0.5f - textSize.x * 0.5f,
            origin.y + drawSize.y * 0.24f);
        const ImVec2 burstCenter(
            origin.x + drawSize.x * 0.5f,
            bannerPosition.y + fontSize * 0.48f);
        const float burstEase =
            1.0f - (1.0f - elapsed) * (1.0f - elapsed) * (1.0f - elapsed);
        const int burstAlpha = static_cast<int>(bannerAlpha * (155.0f - elapsed * 55.0f));

        constexpr int kBurstRayCount = 28;
        for (int ray = 0; ray < kBurstRayCount; ++ray) {
            const float rayRate =
                static_cast<float>(ray) / static_cast<float>(kBurstRayCount);
            const float angle =
                rayRate * kTwoPi + elapsed * 0.42f;
            const float innerRadius = 42.0f + burstEase * 68.0f;
            const float outerRadius =
                innerRadius + 58.0f + PseudoRandom01(ray, 5.7f) * 72.0f;
            drawList->AddLine(
                ImVec2(
                    burstCenter.x + std::cos(angle) * innerRadius,
                    burstCenter.y + std::sin(angle) * innerRadius),
                ImVec2(
                    burstCenter.x + std::cos(angle) * outerRadius,
                    burstCenter.y + std::sin(angle) * outerRadius),
                rainbowColor(rayRate + elapsed * 0.18f, burstAlpha),
                2.0f + PseudoRandom01(ray, 2.3f) * 2.4f);
        }

        constexpr int kBurstRingCount = 3;
        constexpr int kBurstRingArcs = 12;
        for (int ring = 0; ring < kBurstRingCount; ++ring) {
            const float ringRate =
                std::fmod(elapsed + static_cast<float>(ring) * 0.27f, 1.0f);
            const float ringRadius = 62.0f + ringRate * 190.0f;
            const int ringAlpha =
                static_cast<int>(bannerAlpha * (1.0f - ringRate) * 165.0f);
            for (int arc = 0; arc < kBurstRingArcs; ++arc) {
                const float arcRate0 =
                    static_cast<float>(arc) / static_cast<float>(kBurstRingArcs);
                const float arcRate1 =
                    static_cast<float>(arc + 1) / static_cast<float>(kBurstRingArcs);
                drawList->PathArcTo(
                    burstCenter,
                    ringRadius,
                    arcRate0 * kTwoPi + 0.025f,
                    arcRate1 * kTwoPi - 0.025f,
                    8);
                drawList->PathStroke(
                    rainbowColor(arcRate0 + ringRate, ringAlpha),
                    0,
                    2.5f);
            }
        }

        constexpr int kConfettiCount = 24;
        for (int index = 0; index < kConfettiCount; ++index) {
            const float seedX = PseudoRandom01(index, 3.1f);
            const float seedY = PseudoRandom01(index, 7.9f);
            const float fallRate = std::fmod(
                seedY + elapsed * (0.32f + PseudoRandom01(index, 1.4f) * 0.34f),
                1.0f);
            const float x =
                origin.x + drawSize.x * (0.07f + seedX * 0.86f) +
                std::sin(elapsed * 8.0f + static_cast<float>(index)) * 8.0f;
            const float y = origin.y + drawSize.y * (0.05f + fallRate * 0.64f);
            const float width = 3.0f + PseudoRandom01(index, 6.2f) * 4.0f;
            const float height = 6.0f + PseudoRandom01(index, 9.6f) * 7.0f;
            drawList->AddRectFilled(
                ImVec2(x - width, y - height),
                ImVec2(x + width, y + height),
                rainbowColor(seedX + elapsed * 0.24f, static_cast<int>(bannerAlpha * 205.0f)),
                1.5f);
        }

        const ImVec2 plateMin(
            bannerPosition.x - 30.0f,
            bannerPosition.y - 8.0f);
        const ImVec2 plateMax(
            bannerPosition.x + textSize.x + 30.0f,
            bannerPosition.y + fontSize + 26.0f);
        drawList->AddRectFilledMultiColor(
            plateMin,
            plateMax,
            IM_COL32(5, 7, 18, static_cast<int>(bannerAlpha * 62.0f)),
            IM_COL32(15, 5, 22, static_cast<int>(bannerAlpha * 112.0f)),
            IM_COL32(5, 12, 24, static_cast<int>(bannerAlpha * 45.0f)),
            IM_COL32(12, 5, 22, static_cast<int>(bannerAlpha * 96.0f)));
        drawList->AddText(
            ImGui::GetFont(),
            fontSize,
            ImVec2(bannerPosition.x + 3.0f, bannerPosition.y + 3.0f),
            IM_COL32(0, 0, 0, static_cast<int>(210.0f * bannerAlpha)),
            banner);
        float glyphX = bannerPosition.x;
        for (int index = 0; banner[index] != '\0'; ++index) {
            const char glyph[] = { banner[index], '\0' };
            const ImVec2 glyphSize =
                ImGui::GetFont()->CalcTextSizeA(fontSize, 100000.0f, 0.0f, glyph);
            if (banner[index] != ' ') {
                drawList->AddText(
                    ImGui::GetFont(),
                    fontSize,
                    ImVec2(glyphX, bannerPosition.y),
                    rainbowColor(
                        static_cast<float>(index) * 0.105f,
                        static_cast<int>(255.0f * bannerAlpha)),
                    glyph);
            }
            glyphX += glyphSize.x;
        }

        const char* jackpotText = "RAINBOW JACKPOT // SCORE x3";
        const float jackpotFontSize = 15.0f + pulse * 2.0f;
        const ImVec2 jackpotSize = ImGui::GetFont()->CalcTextSizeA(
            jackpotFontSize,
            100000.0f,
            0.0f,
            jackpotText);
        const ImVec2 jackpotPosition(
            burstCenter.x - jackpotSize.x * 0.5f,
            bannerPosition.y + fontSize + 2.0f);
        drawList->AddText(
            ImGui::GetFont(),
            jackpotFontSize,
            ImVec2(jackpotPosition.x + 2.0f, jackpotPosition.y + 2.0f),
            IM_COL32(0, 0, 0, static_cast<int>(220.0f * bannerAlpha)),
            jackpotText);
        drawList->AddText(
            ImGui::GetFont(),
            jackpotFontSize,
            jackpotPosition,
            IM_COL32(255, 245, 182, static_cast<int>(245.0f * bannerAlpha)),
            jackpotText);
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

void GameRuntime::DrawHitConfirmHud()
{
    if (hitConfirmTimer_ <= 0) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const float rate = std::clamp(
        static_cast<float>(hitConfirmTimer_) /
            static_cast<float>((std::max)(hitConfirmDuration_, 1)),
        0.0f,
        1.0f);
    const float elapsed = 1.0f - rate;
    const float settle = 1.0f - std::pow(1.0f - elapsed, 3.0f);
    const float fade = std::clamp(rate * 1.65f, 0.0f, 1.0f);
    const int alpha = static_cast<int>(255.0f * fade);
    const float radius =
        Lerp(32.0f * hitConfirmStrength_, 14.0f, settle) +
        (std::max)(elapsed - 0.72f, 0.0f) * 22.0f;
    const float lineLength = hitConfirmDestroyed_ ? 12.0f : 9.0f;
    const float thickness = hitConfirmDestroyed_ ? 3.0f : 2.2f;
    const ImVec2 center(hitConfirmScreen_.x, hitConfirmScreen_.y);

    const ImU32 mainColor = hitConfirmBoss_ ?
        IM_COL32(255, 126, 230, (std::clamp)(alpha, 0, 255)) :
        hitConfirmDestroyed_ ?
            IM_COL32(255, 220, 112, (std::clamp)(alpha, 0, 255)) :
            hitConfirmCharged_ ?
                IM_COL32(142, 236, 255, (std::clamp)(alpha, 0, 255)) :
                IM_COL32(210, 250, 255, (std::clamp)(alpha, 0, 255));
    const int softAlpha = (std::clamp)(alpha / 3, 0, 96);
    const ImU32 softColor = hitConfirmBoss_ ?
        IM_COL32(255, 64, 192, softAlpha) :
        IM_COL32(64, 198, 255, softAlpha);

    drawList->AddCircle(
        center,
        radius + 5.0f,
        softColor,
        32,
        hitConfirmDestroyed_ ? 4.0f : 3.0f);
    for (int xSign : { -1, 1 }) {
        for (int ySign : { -1, 1 }) {
            const ImVec2 inner(
                center.x + static_cast<float>(xSign) * radius,
                center.y + static_cast<float>(ySign) * radius);
            const ImVec2 outer(
                center.x + static_cast<float>(xSign) * (radius + lineLength),
                center.y + static_cast<float>(ySign) * (radius + lineLength));
            drawList->AddLine(inner, outer, mainColor, thickness);
        }
    }
    drawList->AddCircleFilled(center, hitConfirmDestroyed_ ? 3.2f : 2.4f, mainColor, 16);

    const char* label = hitConfirmBoss_ && hitConfirmDestroyed_ ? "BOSS BREAK" :
        hitConfirmDestroyed_ ? "DESTROY" :
        hitConfirmCharged_ ? "POWER HIT" : "HIT";
    const ImVec2 labelSize = ImGui::CalcTextSize(label);
    const float labelY = center.y + radius + 17.0f;
    drawList->AddText(
        ImVec2(center.x - labelSize.x * 0.5f + 1.0f, labelY + 1.0f),
        IM_COL32(4, 12, 20, (std::clamp)(alpha, 0, 210)),
        label);
    drawList->AddText(
        ImVec2(center.x - labelSize.x * 0.5f, labelY),
        mainColor,
        label);

    if (hitConfirmComboCount_ >= 2) {
        char comboLabel[32]{};
        std::snprintf(
            comboLabel,
            sizeof(comboLabel),
            "CHAIN x%d",
            hitConfirmComboCount_);
        const ImVec2 comboSize = ImGui::CalcTextSize(comboLabel);
        drawList->AddText(
            ImVec2(center.x - comboSize.x * 0.5f, labelY + 17.0f),
            IM_COL32(178, 226, 255, (std::clamp)(alpha * 3 / 4, 0, 220)),
            comboLabel);
    }
}

void GameRuntime::DrawPlayerDamageHud()
{
    if (playerDamageHudTimer_ <= 0) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    Math::Vector2 hudMin{};
    Math::Vector2 hudSize{};
    GetEffectiveHudViewportRect(hudMin, hudSize);
    const float rate = std::clamp(
        static_cast<float>(playerDamageHudTimer_) /
            static_cast<float>((std::max)(playerDamageHudDuration_, 1)),
        0.0f,
        1.0f);
    const float pulse = rate * rate;
    const int edgeAlpha = static_cast<int>(150.0f * pulse);
    const int fillAlpha = static_cast<int>(34.0f * pulse);
    const ImVec2 hudOrigin(hudMin.x, hudMin.y);
    const ImVec2 hudMax(hudMin.x + hudSize.x, hudMin.y + hudSize.y);
    drawList->AddRectFilled(
        hudOrigin,
        hudMax,
        IM_COL32(255, 24, 32, (std::clamp)(fillAlpha, 0, 38)));
    drawList->AddRect(
        ImVec2(hudOrigin.x + 5.0f, hudOrigin.y + 5.0f),
        ImVec2(hudMax.x - 5.0f, hudMax.y - 5.0f),
        IM_COL32(255, 64, 58, (std::clamp)(edgeAlpha, 0, 160)),
        0.0f,
        0,
        4.0f);

    const ImVec2 direction(
        playerDamageDirection_.x,
        playerDamageDirection_.y);
    const ImVec2 tangent(-direction.y, direction.x);
    const ImVec2 arrowCenter(
        playerDamageScreen_.x + direction.x * 82.0f,
        playerDamageScreen_.y + direction.y * 82.0f);
    const ImVec2 arrowTip(
        arrowCenter.x + direction.x * 13.0f,
        arrowCenter.y + direction.y * 13.0f);
    const ImVec2 arrowBaseLeft(
        arrowCenter.x - direction.x * 8.0f + tangent.x * 9.0f,
        arrowCenter.y - direction.y * 8.0f + tangent.y * 9.0f);
    const ImVec2 arrowBaseRight(
        arrowCenter.x - direction.x * 8.0f - tangent.x * 9.0f,
        arrowCenter.y - direction.y * 8.0f - tangent.y * 9.0f);
    const ImU32 dangerColor =
        IM_COL32(255, 98, 72, (std::clamp)(static_cast<int>(245.0f * rate), 0, 245));
    drawList->AddTriangleFilled(
        arrowTip,
        arrowBaseLeft,
        arrowBaseRight,
        dangerColor);
    for (int index = 0; index < 2; ++index) {
        const float offset = 17.0f + static_cast<float>(index) * 9.0f;
        const ImVec2 lineCenter(
            arrowCenter.x - direction.x * offset,
            arrowCenter.y - direction.y * offset);
        drawList->AddLine(
            ImVec2(lineCenter.x + tangent.x * 8.0f, lineCenter.y + tangent.y * 8.0f),
            ImVec2(lineCenter.x - tangent.x * 8.0f, lineCenter.y - tangent.y * 8.0f),
            dangerColor,
            2.5f);
    }
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
        "rail %.1f speed %.3f  stage %.1f pace %.3f  %s / %s",
        railDistance_,
        railSpeed_,
        stageProgress_,
        stageTimelineSpeed_,
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
                AddFeverGauge(
                    isDestroyed ?
                        (isBossHit ? 25 : (isChargedHit ? 16 : 13)) :
                        (isChargedHit ? 4 : 2));
                TriggerHitConfirm(
                    visibleImpactPosition,
                    isChargedHit,
                    isBossHit,
                    isDestroyed);
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
                    AddScore(isBossHit ? 1500 : 100);
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
            const Math::Vector3 incomingVelocity = bullet->GetVelocity();
            const int incomingDamage = bullet->GetDamage();
            bullet->Kill();
            player_->Damage(
                feverTimer_ > 0 ?
                    (std::max)(1, (incomingDamage + 1) / 2) :
                    incomingDamage);
            if (feverTimer_ <= 0) {
                feverGauge_ = (std::max)(0, feverGauge_ - 20);
            }
            AddPlayerDamageEffect(player_->GetTranslate());
            TriggerPlayerDamageFeedback(
                player_->GetTranslate(),
                incomingVelocity);
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
    const float feverSpeedRate = feverSpeedEffectRate_;
    const float feverActivationElapsedFrames =
        static_cast<float>(kFeverActivationFlashFrames - feverActivationFlashTimer_);
    const float feverAccelerationKickPhase =
        std::clamp(feverActivationElapsedFrames / 28.0f, 0.0f, 1.0f);
    const float feverAccelerationKick =
        feverActivationFlashTimer_ > 0 ?
        std::sin(feverAccelerationKickPhase * std::numbers::pi_v<float>) : 0.0f;
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
    const float feverWindBuffetX = feverSpeedRate * (
        std::sin(cameraTimer_ * 0.21f) * 0.040f +
        std::sin(cameraTimer_ * 0.53f + 1.20f) * 0.016f);
    const float feverWindBuffetY = feverSpeedRate * (
        std::sin(cameraTimer_ * 0.27f + 0.70f) * 0.024f +
        std::sin(cameraTimer_ * 0.61f) * 0.010f);
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
            feverWindBuffetX +
            shakeX,
        2.65f +
            playerTranslate.y * (0.04f + justDodgeWhip * kJustDodgeCameraPlayerFollowY) +
            playerVelocity.y * 0.38f +
            railLift +
            stageCameraLiftBias_ +
            std::sin(railDistance_ * 0.017f + 0.35f) * 0.16f +
            justDodgeImpulse * 0.24f +
            justDodgeWhip * (0.08f - kJustDodgeCameraLowAngle) +
            feverSpeedRate * -0.12f +
            bob +
            feverWindBuffetY +
            shakeY,
            railDistance_ -
            (kGameplayCameraBaseDistance +
                speedPulse * 0.55f +
                railSpeedRate * 0.46f +
                feverSpeedRate * (1.15f + speedPulse * 0.25f) +
                feverAccelerationKick * 0.78f +
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
        playerImpactCameraRate * 0.020f +
        feverSpeedRate * 0.035f +
        feverAccelerationKick * 0.025f;
    cameraTranslate_ = Lerp(cameraTranslate_, targetTranslate, cameraTranslateRate);

    const Math::Vector3 targetRotate = {
        0.065f +
            playerTranslate.y * 0.003f +
            railLift * 0.005f +
            speedPulse * 0.004f +
            bob * 0.006f +
            feverWindBuffetY * 0.012f +
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
            feverWindBuffetX * 0.020f +
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
        feverSpeedRate *
            (0.115f + std::sin(cameraTimer_ * 0.11f) * 0.012f) +
        feverAccelerationKick * 0.060f +
        stageCameraFovBoost_ +
        (isGameClear_ ? -0.035f : 0.0f) +
        (isGameOver_ ? 0.025f : 0.0f) +
        cameraShakePower_ * shakeRate * 0.04f;

    cameraRotate_ = Lerp(cameraRotate_, targetRotate, 0.075f + justDodgeCameraRate * 0.082f);
    cameraFovY_ = Lerp(
        cameraFovY_,
        targetFov,
        0.060f + justDodgeCameraRate * 0.080f + feverSpeedRate * 0.040f);

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
