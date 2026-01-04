#pragma once
#include "engine/scene/world/Stage.h"
#include "engine/scene/entities/Player.h"
#include "engine/scene/entities/PlayerController.h"
#include "engine/scene/entities/BulletManager.h"
#include "engine/scene/entities/BossController.h"
#include "engine/scene/entities/Boss.h"

enum class GameState { Title, Playing, Clear, GameOver };

class Input;

class GameScene {
public:
    void Initialize();
    void Update(Input& input, float dt);
    void Draw();

    GameState GetState() const { return state_; }

private:
    void ResetGame();

    GameState state_ = GameState::Title;

    Stage stage_;
    Player player_;
    PlayerController playerCtrl_;
    BulletManager bullets_;
    Boss boss_;
    BossController bossCtrl_;
};
