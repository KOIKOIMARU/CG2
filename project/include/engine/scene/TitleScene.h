#pragma once
#include "engine/scene/BaseScene.h"
#include "engine/scene/SceneType.h"
#include <memory>
#include <array>
#include "engine/base/Math.h"

class Camera;
class Object3d;
class Object3dCommon;
class Skybox;

class TitleScene : public BaseScene {
public:
    TitleScene();
    ~TitleScene() override;

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;
    void RequestStart(bool tutorial = false);

private:
    void PrepareBackdrop();
    void UpdateBackdrop();
    void DrawMenu(bool gameReady);

    std::unique_ptr<Camera> camera_; // タイトル専用。本編のカメラ・機体状態は変更しない。
    std::unique_ptr<Object3dCommon> objectCommon_;
    std::unique_ptr<Skybox> skybox_;
    std::unique_ptr<Object3d> ship_; // 本編の自機と同じモデルを使う、タイトル専用の1機。
    Math::Vector3 modelCenter_{};
    float elapsed_ = 0.0f; // 飛行カットの緩やかな揺れに使う秒数。
    float departureFade_ = 0.0f;
    int selectedItem_ = 0;
    std::array<float, 3> menuEmphasis_{ 1.0f, 0.0f, 0.0f }; // 各項目の選択色。位置・文字サイズは動かさない。
    bool showControls_ = false;
    bool startRequested_ = false;
    SceneType requestedScene_ = SceneType::Game;
};
