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
    int GetPostEffectMode() const { return isGameOver_ ? 1 : 5; }
    bool IsExitRequested() const { return isExitRequested_; }

private:
    void FirePlayerBullet();
    void FireEnemyBullet(const Math::Vector3& position);
    void SpawnEnemy();
    void UpdatePlayerBullets();
    void UpdateEnemyBullets();
    void UpdateEnemies();
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

    Model* playerModel_ = nullptr;
    Model* bulletModel_ = nullptr;
    Model* enemyModel_ = nullptr;

    int shootCooldown_ = 0;
    int enemySpawnTimer_ = 0;
    int enemyShotTimer_ = 60;
    int score_ = 0;
    int defeatedEnemyCount_ = 0;
    int resultTransitionTimer_ = -1;
    int cameraShakeTimer_ = 0;
    int cameraShakeDuration_ = 1;
    float cameraTimer_ = 0.0f;
    float cameraShakePower_ = 0.0f;
    float railDistance_ = 0.0f;
    float railSpeed_ = 0.045f;
    Math::Vector3 cameraTranslate_{ 0.0f, 2.5f, -13.0f };
    Math::Vector3 previousPlayerTranslate_{ 0.0f, 0.0f, 0.0f };
    bool isGameOver_ = false;
    bool isGameClear_ = false;
    bool isDebugGuiVisible_ = true;
    bool isDebugGuiAllowed_ = true;
    bool isExitRequested_ = false;
};
