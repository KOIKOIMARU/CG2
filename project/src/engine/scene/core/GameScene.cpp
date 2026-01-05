#include "engine/scene/core/GameScene.h"
#include "engine/3d/Object3dCommon.h" 
#include "engine/io/Input.h"
#include "engine/scene/world/Collision.h"  
#include "engine/3d/ModelManager.h"
#include <array> 
#include <cassert>


// いったん仮：後でクラスにする
// extern void DrawEverythingLikeMain(...); みたいにしてもOK

void GameScene::Initialize(Object3dCommon* objCommon)
{
    objCommon_ = objCommon;

    // ★追加：ステージのブロックObjectを作る
    stage_.Initialize(objCommon_);

    playerObj_.Initialize(objCommon_);
    playerObj_.SetModel(ModelManager::GetInstance()->FindModel("player.obj"));

    bossObj_.Initialize(objCommon_);
    bossObj_.SetModel(ModelManager::GetInstance()->FindModel("enemy.obj"));

    auto* bulletModel = ModelManager::GetInstance()->FindModel("bullet.obj");
    for (auto& o : bulletObjs_) {
        o.Initialize(objCommon_);
        o.SetModel(bulletModel);
    }

    state_ = GameState::Title;
    ResetGame();
}

void GameScene::ResetGame()
{
    player_ = Player{};
    player_.pos = { 2.0f, 2.0f, 0.0f };

    boss_ = Boss{};
    bullets_.Clear();
}

extern Math::Vector3 kPlayerSize;
static AABB MakePlayerAABB(const Player& p) {
    return { p.pos.x, p.pos.y, kPlayerSize.x, kPlayerSize.y };
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
    {
        // プレイヤー更新（Controller統合版）
        player_.Update(stage_, input, dt);

        // 弾生成（Player内の要求を消費）
        Math::Vector3 pos, vel;
        float power, damage, life;
        if (player_.ConsumeShotRequest(pos, vel, power, damage, life)) {
            bullets_.Spawn(pos, vel, power, damage, life);
        }

        bullets_.Update(dt);

        // ボス更新（Controller統合版）
        boss_.Update(player_, stage_, dt);

        // 弾→ボス当たり判定
        bullets_.CheckHitBoss(boss_);

        if (!boss_.IsAlive()) {
            state_ = GameState::Clear;
        }

        // --------------------
        // Player vs Boss 接触ダメージ（ここはそのままでOK）
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
            player_.hp -= 10.0f;
            if (player_.hp < 0.0f) player_.hp = 0.0f;

            player_.invincible = true;
            player_.invincibleTime = 0.0f;

            float dir = (player_.pos.x < boss_.pos_.x) ? -1.0f : +1.0f;
            player_.vel.x = dir * player_.knockbackPower;
            player_.vel.y = player_.knockbackUp;

            if (player_.hp <= 0.0f) {
                state_ = GameState::GameOver;
                bullets_.Clear();
            }
        }

        break;
    }


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

    // =====================
// 見た目(Object3d)へ反映
// =====================
    playerObj_.SetTranslate({ player_.pos.x, player_.pos.y, 0.0f });
    bossObj_.SetTranslate({ boss_.pos_.x, boss_.pos_.y, 0.0f });

    // 弾のObject3dを弾情報に同期
    const auto& bs = bullets_.GetBullets();

    for (size_t i = 0; i < bulletObjs_.size(); ++i) {
        if (bs[i].alive) {
            bulletObjs_[i].SetTranslate({ bs[i].pos.x, bs[i].pos.y, 0.0f });
            bulletObjs_[i].SetScale({ bs[i].size.x, bs[i].size.y, 1.0f });
        } else {
            bulletObjs_[i].SetTranslate({ 99999.0f, 99999.0f, 0.0f });
        }
    }


    // WVP更新
    playerObj_.Update();
    bossObj_.Update();
    for (auto& o : bulletObjs_) o.Update();


}

void GameScene::Draw()
{
    objCommon_->CommonDrawSetting();

    // ★追加：まずステージ描画
    stage_.Draw();

    playerObj_.Draw();
    bossObj_.Draw();

    const auto& bs = bullets_.GetBullets();
    for (size_t i = 0; i < bulletObjs_.size(); ++i) {
        if (bs[i].alive) {
            bulletObjs_[i].Draw();
        }
    }
}
