#include "engine/effect/PrimitiveEffect.h"

#include <cassert>

#include "engine/3d/Object3d.h"

void PrimitiveEffect::Initialize(
    Object3dCommon* object3dCommon,
    const Math::Vector4& cylinderColor,
    float cylinderAlphaReference,
    float planeEnvironmentCoefficient)
{
    assert(object3dCommon);

    plane_ = std::make_unique<Object3d>();
    plane_->Initialize(object3dCommon);
    plane_->SetModel("primitive_plane");
    plane_->SetEnvironmentCoefficient(planeEnvironmentCoefficient);
    plane_->SetRotate({ 1.5707963f, 0.0f, 0.0f });

    ring_ = std::make_unique<Object3d>();
    ring_->Initialize(object3dCommon);
    ring_->SetModel("primitive_ring");
    ring_->SetTranslate({ 0.0f, 0.05f, 0.0f });
    ring_->SetRotate({ 1.5707963f, 0.0f, 0.0f });
    ring_->SetEnvironmentCoefficient(0.0f);

    cylinder_ = std::make_unique<Object3d>();
    cylinder_->Initialize(object3dCommon);
    cylinder_->SetModel("primitive_cylinder");
    cylinder_->SetTranslate({ 0.0f, 0.0f, 0.0f });
    cylinder_->SetEnvironmentCoefficient(0.0f);
    cylinder_->SetLightingMode(0);
    cylinder_->SetColor(cylinderColor);
    cylinder_->SetAlphaReference(cylinderAlphaReference);
}

void PrimitiveEffect::Update(
    float deltaTime,
    const Math::Vector3& sharedRotate,
    const Math::Vector4& cylinderColor,
    float cylinderAlphaReference,
    float cylinderUVScrollSpeed)
{
    if (!plane_ || !ring_ || !cylinder_) {
        return;
    }

    plane_->SetRotate(sharedRotate);
    ring_->SetRotate({ 1.5707963f, sharedRotate.y, sharedRotate.z });

    cylinderUVOffset_ += cylinderUVScrollSpeed * deltaTime;
    const Math::Matrix4x4 cylinderUVTransform = Math::Multiply(
        Math::MakeScaleMatrix({ 1.0f, -1.0f, 1.0f }),
        Math::MakeTranslateMatrix({ cylinderUVOffset_, 1.0f, 0.0f })
    );
    cylinder_->SetColor(cylinderColor);
    cylinder_->SetAlphaReference(cylinderAlphaReference);
    cylinder_->SetUVTransform(cylinderUVTransform);

    plane_->Update();
    ring_->Update();
    cylinder_->Update();
}

void PrimitiveEffect::Draw(bool showPlane, bool showRing, bool showCylinder)
{
    if (showPlane && plane_) {
        plane_->Draw();
    }
    if (showRing && ring_) {
        ring_->Draw();
    }
    if (showCylinder && cylinder_) {
        cylinder_->Draw();
    }
}
