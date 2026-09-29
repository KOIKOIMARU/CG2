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
class SoundManager;

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
    void UpdateFlightEffects(const Math::Vector3& center, const Math::Matrix4x4& rotation, float thrust);
    void DrawMenu(bool gameReady);

    std::unique_ptr<Camera> camera_; // タイトル専用。本編のカメラ・機体状態は変更しない。
    std::unique_ptr<Object3dCommon> objectCommon_;
    std::unique_ptr<Skybox> skybox_;
    std::unique_ptr<Object3d> ship_; // 本編の自機と同じモデルを使う、タイトル専用の1機。
    std::array<std::unique_ptr<Object3d>, 6> exhaust_; // 左右ノズルの外炎・内炎・発光。初期化時だけ確保する。
    std::unique_ptr<SoundManager> sound_; // 出撃の決定音・加速音。本編の再生状態とは分離する。
    Math::Vector3 modelCenter_{};
    float elapsed_ = 0.0f; // モデル準備後の演出時間。ロード待ちで導入を飛ばさない。
    float menuRevealTime_ = 0.0f; // 文字の導入はロードと独立。モデル出現時に文字を再点滅させない。
    float departureTime_ = 0.0f; // 出撃確定後の加速カット。秒単位で進める。
    float departureFade_ = 0.0f;
    int selectedItem_ = 0;
    std::array<float, 3> menuEmphasis_{ 1.0f, 0.0f, 0.0f }; // 各項目の選択色。位置・文字サイズは動かさない。
    bool showControls_ = false;
    bool startRequested_ = false;
    SceneType requestedScene_ = SceneType::Game;
};
