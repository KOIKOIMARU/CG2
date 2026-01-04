#include "engine/scene/core/GameScene.h"
#include "engine/io/Input.h"
#include "engine/scene/world/Collision.h"  

// いったん仮：後でクラスにする
// extern void DrawEverythingLikeMain(...); みたいにしてもOK

void GameScene::Initialize()
{
    state_ = GameState::Title;
}

void GameScene::ResetGame()
{
    player_ = Player{};
    player_.pos = { 2.0f, 2.0f, 0.0f };

    boss_ = Boss{};
    bullets_.Clear();
}

static AABB MakePlayerAABB(const Player& p) {
    return { p.pos.x, p.pos.y, Player::Size.x, Player::Size.y };
}


static AABB MakeBossAABB(const Boss& b) {
    return { b.pos_.x, b.pos_.y, b.sizeX_, b.sizeY_ };
}

void GameScene::Update(Input& input, float dt)
{
    // 共通：ESCでタイトルへ
    if (input.TriggerKey(DIK_ESCAPE)) {
        ResetGame();              // ★追加（中で bullets_.Clear() もされる）
        state_ = GameState::Title;
    }



    switch (state_) {
    case GameState::Title:
        if (input.TriggerKey(DIK_SPACE)) {
            ResetGame();
            state_ = GameState::Playing;
        }
        break;

    case GameState::Playing:
        // プレイヤー更新
        playerCtrl_.Update(player_, stage_, input, dt);

        // 弾生成（次にBulletManager作ったらここに置く）
        Math::Vector3 pos, vel;
        float power, damage, life;

        if (playerCtrl_.ConsumeShotRequest(pos, vel, power, damage, life)) {
            bullets_.Spawn(pos, vel, power, damage, life);
        }

        bullets_.Update(dt);
        bossCtrl_.Update(boss_, player_, stage_, dt);
        bullets_.CheckHitBoss(boss_);

        if (!boss_.IsAlive()) {
            state_ = GameState::Clear;
        }

        // --------------------
        // Player vs Boss 接触ダメージ
        // --------------------
        player_.blinkTimer += dt;

        if (player_.invincible) {
            player_.invincibleTime += dt;
            if (player_.invincibleTime >= player_.invincibleDuration) {
                player_.invincible = false;
                player_.invincibleTime = 0.0f;
            }
        }

        AABB aP = MakePlayerAABB(player_);
        AABB aB = MakeBossAABB(boss_);

        if (boss_.IsAlive() && !player_.invincible && Collision::IntersectAABB(aP, aB)) {
            // ダメージ
            player_.hp -= 10.0f;
            if (player_.hp < 0.0f) player_.hp = 0.0f;

            // 無敵開始
            player_.invincible = true;
            player_.invincibleTime = 0.0f;

            // ノックバック
            float dir = (player_.pos.x < boss_.pos_.x) ? -1.0f : +1.0f;
            player_.vel.x = dir * player_.knockbackPower;
            player_.vel.y = player_.knockbackUp;

            if (player_.hp <= 0.0f) {
                state_ = GameState::GameOver;
                bullets_.Clear();
            }
        }


        // クリア/ゲームオーバー判定もここ
        break;

    case GameState::Clear:
        if (input.TriggerKey(DIK_SPACE)) {
            ResetGame();              // ★追加
            state_ = GameState::Title;
        }
        break;

    case GameState::GameOver:
        if (input.TriggerKey(DIK_SPACE)) {
            ResetGame();              // ★追加
            state_ = GameState::Title;
        }
        break;

    }

}

void GameScene::Draw()
{
    // いま main でやってる描画を
    // 「GameSceneが持ってるデータ」を使って呼ぶ形に寄せていく

    // 例：
    // DrawStage(stage_);
    // DrawPlayer(player_);
    // DrawBoss(...)
    // DrawBullets(...)
    // DrawUI(state_, player_, boss_...)
}

