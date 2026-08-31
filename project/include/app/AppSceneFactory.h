#pragma once
#include "engine/scene/AbstractSceneFactory.h"

// エンジンの汎用シーン管理へ、このアプリケーションで使うシーンを登録する。
// ゲーム固有クラスを生成する責務は engine 層ではなく、この構成ルートに置く。
class AppSceneFactory final : public AbstractSceneFactory {
public:
    ~AppSceneFactory() override = default;
    std::unique_ptr<BaseScene> CreateScene(SceneType sceneType) override;
};
