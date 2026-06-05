#pragma once

#include <memory>

#include "engine/base/Math.h"

class Object3d;
class Object3dCommon;

class PrimitiveEffect {
public:
    void Initialize(
        Object3dCommon* object3dCommon,
        const Math::Vector4& cylinderColor,
        float cylinderAlphaReference,
        float planeEnvironmentCoefficient);
    void Update(
        float deltaTime,
        const Math::Vector3& sharedRotate,
        const Math::Vector4& cylinderColor,
        float cylinderAlphaReference,
        float cylinderUVScrollSpeed);
    void Draw(bool showPlane, bool showRing, bool showCylinder);

    Object3d* GetPlane() const { return plane_.get(); }
    Object3d* GetRing() const { return ring_.get(); }
    Object3d* GetCylinder() const { return cylinder_.get(); }

private:
    std::unique_ptr<Object3d> plane_;
    std::unique_ptr<Object3d> ring_;
    std::unique_ptr<Object3d> cylinder_;
    float cylinderUVOffset_ = 0.0f;
};
