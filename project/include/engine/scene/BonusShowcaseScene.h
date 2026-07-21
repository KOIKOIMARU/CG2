#pragma once

#include <memory>
#include <vector>

#include "engine/base/Math.h"
#include "engine/scene/BaseScene.h"

class Camera;
class Object3d;
class Object3dCommon;
class Skybox;

class BonusShowcaseScene : public BaseScene {
public:
    BonusShowcaseScene();
    ~BonusShowcaseScene() override;

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;

private:
    std::unique_ptr<Object3d> CreateDisplayObject(
        const char* modelName,
        const Math::Vector3& translate,
        const Math::Vector3& scale);
    void InitializeBoneDebug();
    void UpdateBoneDebug();
    void DrawShowcaseHud();

private:
    std::unique_ptr<Object3dCommon> object3dCommon_;
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<Skybox> skybox_;
    std::unique_ptr<Object3d> simpleSkinObject_;
    std::unique_ptr<Object3d> humanWalkObject_;
    std::unique_ptr<Object3d> multiMeshObject_;
    std::unique_ptr<Object3d> multiMaterialObject_;
    std::vector<std::unique_ptr<Object3d>> jointDebugObjects_;
    std::vector<std::unique_ptr<Object3d>> boneDebugObjects_;
    bool showBoneDebug_ = true;
    float demonstrationTime_ = 0.0f;
};
