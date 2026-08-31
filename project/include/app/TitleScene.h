#pragma once
#include "engine/scene/BaseScene.h"
#include <memory>
#include <vector>
#include "engine/base/Math.h"
#include "engine/2d/Sprite.h"

// レールシューティングサンプル専用のタイトル画面。
// 通常のチームエンジン起動では使用せず、自動スモークテスト時だけ経由する。
class TitleScene : public BaseScene {
public:
    TitleScene();
    ~TitleScene() override;

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;

private:
    std::vector<Sprite> sprites_;
    Math::Vector2 spritePos_{ 200.0f, 120.0f };
    bool startRequested_ = false;
};
