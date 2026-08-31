#pragma once
#include <memory>
#include "engine/scene/SceneType.h"

class BaseScene;

// SceneManagerからアプリケーション固有シーンの生成処理を切り離す抽象ファクトリ。
// 実装はapp層へ置き、engine層から具体的なシーンクラスを参照させない。
class AbstractSceneFactory {
public:
    virtual ~AbstractSceneFactory() = default;
    // 指定種別の新しいシーンを返す。所有権はSceneManagerへ移る。
    virtual std::unique_ptr<BaseScene> CreateScene(SceneType sceneType) = 0;
};
