#pragma once
#include "engine/base/Framework.h"

#include <vector>
#include "engine/base/Math.h"
#include "engine/2d/Sprite.h"

// forward
class Object3dCommon;
class Object3d;
class Camera;

class Sprite;
class ParticleEmitter;

class MyGame : public Framework {
public:
    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;

private:
    // ゲーム固有
    Object3dCommon* object3dCommon_ = nullptr;

    Camera* camera_ = nullptr;
    Object3d* object3d_ = nullptr;

    std::vector<Sprite> sprites_;
    Math::Vector2 spritePos_{ 100.0f, 100.0f };

    // ※ParticleEmitterが「値型」で持てるなら値でOK
    //   もしコピー不可なら unique_ptr にする
    ParticleEmitter* emitter_ = nullptr;

    // サウンド（今回の実装はMyGame側でOK）
    // SoundManager sound_;  ← SoundManagerが値型でOKならこれ
};
