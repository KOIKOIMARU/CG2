#include "engine/3d/Camera.h"
#include "engine/base/WinApp.h"

using namespace Math;

Camera::Camera()
{
    transform = {
        {1.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f}
    };

    fovY = 0.45f;
    aspectRatio = float(WinApp::kClientWidth) / float(WinApp::kClientHeight);
    nearClip = 0.1f;
    farClip = 100.0f;

    Update();
}


void Camera::Update()
{
    // ★ 資料通り：まず worldMatrix
    worldMatrix = MakeAffineMatrix(
        transform.scale,
        transform.rotate,
        transform.translate
    );

    // ★ view は world の逆行列
    viewMatrix = Inverse(worldMatrix);

    // ★ projection
    projectionMatrix = MakePerspectiveFovMatrix(
        fovY,
        aspectRatio,
        nearClip,
        farClip
    );

    // ★ VP
    viewProjectionMatrix =
        Multiply(viewMatrix, projectionMatrix);
}



// ===== setter =====
void Camera::SetRotate(const Vector3& rotate) {
    transform.rotate = rotate;
}

void Camera::SetTranslate(const Vector3& translate) {
    transform.translate = translate;
}

void Camera::SetFovY(float value) {
    fovY = value;
}

void Camera::SetAspectRatio(float value) {
    aspectRatio = value;
}

void Camera::SetNearClip(float value) {
    nearClip = value;
}

void Camera::SetFarClip(float value) {
    farClip = value;
}

// ===== getter =====
const Matrix4x4& Camera::GetWorldMatrix() const {
    return worldMatrix;
}

const Matrix4x4& Camera::GetViewMatrix() const {
    return viewMatrix;
}

const Matrix4x4& Camera::GetProjectionMatrix() const {
    return projectionMatrix;
}

const Matrix4x4& Camera::GetViewProjectionMatrix() const {
    return viewProjectionMatrix;
}

const Vector3& Camera::GetRotate() const {
    return transform.rotate;
}

const Vector3& Camera::GetTranslate() const {
    return transform.translate;
}
