#pragma once
#include <memory>
#include "engine/scene/SceneType.h"

class BaseScene;
class AbstractSceneFactory;
class DirectXCommon;
class SrvManager;
class SpriteCommon;
class ImGuiManager;
class Input;

// シーンの生成、初期化、更新、破棄と遷移を一元管理するシングルトン。
// SetNextScene は次回 Update の先頭で安全に切り替えるための予約である。
class SceneManager {
public:
    static SceneManager* GetInstance();

    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    ~SceneManager();

    void Update();
    void Draw();
    BaseScene* GetCurrentScene() const { return scene_.get(); }
    void FinalizeCurrentScene();

    void SetNextScene(SceneType sceneType);
    // 重い初期化を遷移前に済ませたい場合の事前生成。現在シーンは維持される。
    void PrepareScene(SceneType sceneType);
    bool IsScenePrepared(SceneType sceneType) const;
    void SetSceneFactory(AbstractSceneFactory* sceneFactory);

    void SetSystems(
        DirectXCommon* dxCommon,
        SrvManager* srvManager,
        SpriteCommon* spriteCommon,
        ImGuiManager* imguiManager,
        Input* input
    );

private:
    SceneManager() = default;

private:
    std::unique_ptr<BaseScene> scene_;         // 現在、更新と描画を行う所有シーン
    std::unique_ptr<BaseScene> preparedScene_; // 遷移前に初期化済みの所有シーン
    SceneType preparedSceneType_{};            // preparedScene_の種類
    bool hasPreparedScene_ = false;            // 事前生成済みシーンが利用可能か
    SceneType nextSceneType_{};                // 次回Updateで切り替えるシーン種類
    bool hasNextScene_ = false;                // シーン遷移の予約があるか

    AbstractSceneFactory* sceneFactory_ = nullptr; // シーン生成規則の借用先

    DirectXCommon* dxCommon_ = nullptr;    // 新しいシーンへ渡す描画基盤
    SrvManager* srvManager_ = nullptr;     // 新しいシーンへ渡すSRV管理
    SpriteCommon* spriteCommon_ = nullptr; // 新しいシーンへ渡す2D描画基盤
    ImGuiManager* imguiManager_ = nullptr; // 新しいシーンへ渡すUI基盤
    Input* input_ = nullptr;               // 新しいシーンへ渡す入力
};
