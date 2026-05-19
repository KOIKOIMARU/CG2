#pragma once

#include "app/Bullet.h"
#include "app/Enemy.h"
#include "app/Player.h"
#include "engine/3d/Camera.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/scene/BaseScene.h"

#include <list>
#include <memory>

class Model;

class GameScene : public BaseScene {
public:
    GameScene();
    ~GameScene() override;

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;
    int GetPostEffectMode() const { return isGameOver_ ? 1 : 0; }

private:
    void FirePlayerBullet();
    void FireEnemyBullet(const Math::Vector3& position);
    void SpawnEnemy();
    void UpdatePlayerBullets();
    void UpdateEnemyBullets();
    void UpdateEnemies();
    void CheckBulletEnemyCollisions();
    void CheckEnemyBulletPlayerCollisions();

    std::unique_ptr<Object3dCommon> object3dCommon_;
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<Object3d> guidePlane_;
    std::unique_ptr<Player> player_;
    std::list<std::unique_ptr<Bullet>> playerBullets_;
    std::list<std::unique_ptr<Bullet>> enemyBullets_;
    std::list<std::unique_ptr<Enemy>> enemies_;

    Model* playerModel_ = nullptr;
    Model* bulletModel_ = nullptr;
    Model* enemyModel_ = nullptr;
    Model* guidePlaneModel_ = nullptr;

    int shootCooldown_ = 0;
    int enemySpawnTimer_ = 0;
    int enemyShotTimer_ = 60;
    int score_ = 0;
    bool isGameOver_ = false;
};
