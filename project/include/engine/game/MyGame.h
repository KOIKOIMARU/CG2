#pragma once
#include "engine/base/Framework.h"

#include <vector>
#include <cstdint>
#include <wrl/client.h>

#include "engine/base/WinApp.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/SrvManager.h"
#include "engine/base/ImGuiManager.h"
#include "engine/io/Input.h"
#include "engine/2d/SpriteCommon.h"
#include "engine/2d/Sprite.h"
#include "engine/3d/TextureManager.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/ModelCommon.h"
#include "engine/3d/ModelManager.h"
#include "engine/3d/Camera.h"
#include "engine/3d/ParticleManager.h"
#include "engine/3d/ParticleEmitter.h"
#include "engine/audio/SoundManager.h"
#include "engine/base/Math.h"

using namespace Math; // 使ってるなら


class MyGame : public Framework {
public:
    ~MyGame() override = default;

protected:
    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;

private:
    // --- 基盤 ---
    WinApp* winApp_ = nullptr;
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    ImGuiManager* imguiManager_ = nullptr;
    Input* input_ = nullptr;

    // --- 2D ---
    SpriteCommon* spriteCommon_ = nullptr;
    std::vector<Sprite> sprites_;
    Vector2 spritePos_{ 100.0f, 100.0f };

    // --- 3D ---
    Object3dCommon* object3dCommon_ = nullptr;
    ModelCommon* modelCommon_ = nullptr;
    Camera* camera_ = nullptr;
    Object3d* object3d_ = nullptr;

    // --- Particle ---
    ParticleEmitter* emitter_ = nullptr;

    // --- Sound ---
    SoundManager sound_;

    // --- DXから取ってたやつ（今はほぼ不要なので「必要になったら」追加でOK） ---
    HANDLE fenceEvent_ = nullptr; // CloseHandleしてたので保持
};
