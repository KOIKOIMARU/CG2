#pragma once
#include "engine/scene/BaseScene.h"
#include <memory>
#include <vector>
#include "engine/base/Math.h"
#include "engine/2d/Sprite.h"

class Object3dCommon;
class Object3d;
class Camera;
class ParticleEmitter;

class GamePlayScene : public BaseScene {
public:
    GamePlayScene();
    ~GamePlayScene() override;

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;

private:
    std::unique_ptr<Object3dCommon> object3dCommon_;
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<Object3d> object3d_;
    std::unique_ptr<ParticleEmitter> emitter_;

    std::vector<Sprite> sprites_;
    Math::Vector2 spritePos_{ 100.0f, 100.0f };
};