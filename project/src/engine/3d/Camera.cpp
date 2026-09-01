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
    // カメラの姿勢からワールド行列を組み立てる。
    worldMatrix = MakeAffineMatrix(
        transform.scale,
        transform.rotate,
        transform.translate
    );

    // カメラのワールド変換を逆にしてビュー変換を求める。
    viewMatrix = Inverse(worldMatrix);

    // ゲームの見せ方に応じて透視投影と正投影を切り替える。
    if (projectionMode == CameraProjectionMode::Orthographic) {
        const float halfHeight = orthographicHeight * 0.5f;
        const float halfWidth = halfHeight * aspectRatio;
        projectionMatrix = MakeOrthographicMatrix(
            -halfWidth,
            halfHeight,
            halfWidth,
            -halfHeight,
            nearClip,
            farClip);
    } else {
        projectionMatrix = MakePerspectiveFovMatrix(
            fovY,
            aspectRatio,
            nearClip,
            farClip);
    }

    // 描画側が直接利用できるようビューと投影を合成する。
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

void Camera::SetProjectionMode(CameraProjectionMode mode) {
    projectionMode = mode;
}

void Camera::SetOrthographicHeight(float value) {
    if (value > 0.0f) {
        orthographicHeight = value;
    }
}

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

CameraProjectionMode Camera::GetProjectionMode() const {
    return projectionMode;
}

float Camera::GetOrthographicHeight() const {
    return orthographicHeight;
}
