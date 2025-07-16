
#pragma once
#include "MathTypes.h"
#include <cstdint>


class DebugCamera {
public:
    void Initialize(const Vector3& pos, const Vector3& rotation);
    void Update(const uint8_t* keys);

    Vector3 GetPosition() const { return position_; }
    const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }
    const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }

private:
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;

    Vector3 position_ = { 0.0f, 0.0f, -5.0f };

    Matrix4x4 viewMatrix_;
    Matrix4x4 projectionMatrix_;
};
