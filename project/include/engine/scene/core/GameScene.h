#pragma once
#include <array> 
#include "engine/scene/world/Stage.h"
#include "engine/scene/entities/Player.h"
#include "engine/scene/entities/BulletManager.h"
#include "engine/scene/entities/Boss.h"
#include "engine/3d/Object3d.h"

enum class GameState { Title, Playing, Clear, GameOver };

class Input;
class Object3dCommon;

class GameScene {
public:
    void Initialize(Object3dCommon* objCommon);
    void Update(Input& input, float dt);
    void Draw();

    GameState GetState() const { return state_; }

private:
    void ResetGame();

    GameState state_ = GameState::Title;

    Stage stage_;
    Player player_;
    BulletManager bullets_;
    Boss boss_;

    Object3dCommon* objCommon_ = nullptr;

    Object3d playerObj_;
    Object3d bossObj_;
    std::array<Object3d, BulletManager::kMaxBullets> bulletObjs_;
};
