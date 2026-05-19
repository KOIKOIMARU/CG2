#include "app/GameScene.h"

#include "engine/3d/ModelManager.h"
#include "engine/3d/TextureManager.h"
#include "engine/io/Input.h"
#include "engine/scene/SceneManager.h"

#include <imgui.h>

namespace {

float DistanceSquared(const Math::Vector3& a, const Math::Vector3& b)
{
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    const float z = a.z - b.z;
    return x * x + y * y + z * z;
}

} // namespace

GameScene::GameScene() = default;
GameScene::~GameScene() = default;

void GameScene::Initialize()
{
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
        "game_guide_plane",
        14.0f,
        60.0f,
        "resources/checkerBoard.png");

    playerModel_ = ModelManager::GetInstance()->FindModel("game_player");
    bulletModel_ = ModelManager::GetInstance()->FindModel("game_bullet");
    enemyModel_ = ModelManager::GetInstance()->FindModel("game_enemy");
    guidePlaneModel_ = ModelManager::GetInstance()->FindModel("game_guide_plane");

    camera_ = std::make_unique<Camera>();
    camera_->SetRotate({ 0.15f, 0.0f, 0.0f });
    camera_->SetTranslate({ 0.0f, 2.5f, -13.0f });
    object3dCommon_->SetDefaultCamera(camera_.get());

    guidePlane_ = std::make_unique<Object3d>();
    guidePlane_->Initialize(object3dCommon_.get());
    guidePlane_->SetModel(guidePlaneModel_);
    guidePlane_->SetRotate({ 1.5707963f, 0.0f, 0.0f });
    guidePlane_->SetTranslate({ 0.0f, -3.2f, 18.0f });
    guidePlane_->SetColor({ 0.45f, 0.45f, 0.55f, 1.0f });
    guidePlane_->Update();

    player_ = std::make_unique<Player>();
    player_->Initialize(object3dCommon_.get(), playerModel_);
}

void GameScene::Finalize()
{
    playerBullets_.clear();
    enemyBullets_.clear();
    enemies_.clear();
    player_.reset();
    guidePlane_.reset();
    camera_.reset();
    object3dCommon_.reset();
    ModelManager::GetInstance()->Finalize();
}

void GameScene::Update()
{
    if (input_ && input_->TriggerKey(DIK_F2)) {
        sceneManager_->SetNextScene(SceneType::GamePlay);
        return;
    }

    camera_->Update();
    player_->Update(input_);

    if (!isGameOver_) {
        if (shootCooldown_ > 0) {
            --shootCooldown_;
        }
        if (input_ && input_->PushKey(DIK_SPACE) && shootCooldown_ <= 0) {
            FirePlayerBullet();
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

    if (guidePlane_) {
        guidePlane_->Update();
    }

    ImGui::Begin("Game Debug");
    ImGui::Text("Player HP: %d", player_->GetHp());
    ImGui::Text("Score: %d", score_);
    ImGui::Text("Player Bullets: %zu", playerBullets_.size());
    ImGui::Text("Enemy Bullets: %zu", enemyBullets_.size());
    ImGui::Text("Enemies: %zu", enemies_.size());
    if (isGameOver_) {
        ImGui::TextUnformatted("GAME OVER - grayscale post effect");
    }
    ImGui::End();
}

void GameScene::Draw()
{
    object3dCommon_->CommonDrawSetting();
    if (guidePlane_) {
        guidePlane_->Draw();
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

void GameScene::FirePlayerBullet()
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

void GameScene::FireEnemyBullet(const Math::Vector3& position)
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

void GameScene::SpawnEnemy()
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

void GameScene::UpdatePlayerBullets()
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

void GameScene::UpdateEnemyBullets()
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

void GameScene::UpdateEnemies()
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

void GameScene::CheckBulletEnemyCollisions()
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
                break;
            }
        }
    }
}

void GameScene::CheckEnemyBulletPlayerCollisions()
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
            if (player_->IsDead()) {
                isGameOver_ = true;
            }
            break;
        }
    }
}
