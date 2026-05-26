#include "app/GameRuntime.h"

#include "engine/3d/ModelManager.h"
#include "engine/3d/TextureManager.h"
#include "engine/io/Input.h"
#include "engine/scene/SceneSerializer.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace {

constexpr const char* kGameSceneFilePath = "resources/game_scene.json";

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
    object3dCommon_->SetDefaultCamera(camera_.get());

    player_ = std::make_unique<Player>();
    player_->Initialize(object3dCommon_.get(), playerModel_);
    previousPlayerTranslate_ = player_->GetTranslate();
    cameraTranslate_ = camera_->GetTranslate();

    LoadSceneObjects(kGameSceneFilePath);
}

void GameRuntime::Finalize()
{
    sceneObjects_.clear();
    playerBullets_.clear();
    enemyBullets_.clear();
    enemies_.clear();
    player_.reset();
    camera_.reset();
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

    player_->Update(input_);
    UpdateGameCamera();

    if (!isGameOver_ && !isGameClear_) {
        if (shootCooldown_ > 0) {
            --shootCooldown_;
        }
        if (input_ && input_->PushKey(DIK_SPACE) && shootCooldown_ <= 0) {
            FirePlayerBullet();
            AddCameraShake(0.035f, 8);
            shootCooldown_ = 8;
        }

        --enemySpawnTimer_;
        if (enemySpawnTimer_ <= 0) {
            SpawnEnemy();
            enemySpawnTimer_ = 90;
        }

        --enemyShotTimer_;
        if (enemyShotTimer_ <= 0) {
            for (const auto& enemy : enemies_) {
                if (!enemy->IsDead()) {
                    FireEnemyBullet(enemy->GetTranslate());
                }
            }
            enemyShotTimer_ = 75;
        }
    }

    UpdatePlayerBullets();
    UpdateEnemyBullets();
    UpdateEnemies();
    CheckBulletEnemyCollisions();
    CheckEnemyBulletPlayerCollisions();

    if (resultTransitionTimer_ > 0) {
        --resultTransitionTimer_;
        if (resultTransitionTimer_ == 0) {
            isExitRequested_ = true;
            return;
        }
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
        ImGui::Text("撃破数: %d / 20", defeatedEnemyCount_);
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
    object3dCommon_->CommonDrawSetting();
    for (const auto& sceneObject : sceneObjects_) {
        sceneObject->Draw();
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

    auto bullet = std::make_unique<Bullet>();
    bullet->Initialize(
        object3dCommon_.get(),
        bulletModel_,
        spawnPosition,
        { 0.0f, 0.0f, 0.65f });
    playerBullets_.push_back(std::move(bullet));
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
    if (!enemyModel_) {
        return;
    }

    static int spawnIndex = 0;
    const float xPositions[] = { -4.0f, -2.0f, 0.0f, 2.0f, 4.0f };
    const float yPositions[] = { -1.5f, 0.0f, 1.5f };
    const float x = xPositions[spawnIndex % 5];
    const float y = yPositions[(spawnIndex / 5) % 3];
    ++spawnIndex;

    auto enemy = std::make_unique<Enemy>();
    enemy->Initialize(
        object3dCommon_.get(),
        enemyModel_,
        { x, y, 28.0f });
    enemies_.push_back(std::move(enemy));
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
        (*iterator)->Update();
        if ((*iterator)->IsDead()) {
            iterator = enemies_.erase(iterator);
        } else {
            ++iterator;
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
                bullet->Kill();
                enemy->Kill();
                score_ += 100;
                ++defeatedEnemyCount_;
                if (defeatedEnemyCount_ >= 20 && !isGameClear_) {
                    isGameClear_ = true;
                    resultTransitionTimer_ = 90;
                }
                break;
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
        playerTranslate.z - previousPlayerTranslate_.z,
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
        -13.8f + resultZoom,
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
