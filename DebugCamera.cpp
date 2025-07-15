#define NOMINMAX
#include <Windows.h>
#include <dinput.h>
#include <cmath>
#include <algorithm> 

#include "DebugCamera.h"

//Max関数
template<typename T>
T Max(T a, T b) {
    return (a > b) ? a : b;
}

void DebugCamera::Initialize() {
    theta_ = 3.141592f;
    phi_ = 0.0f;
    distance_ = 5.0f;
    pivot_ = { 0.0f, 0.0f, 0.0f };
}

void DebugCamera::Update(const uint8_t* keys) {
    const float rotateSpeed = 0.02f;
    const float moveSpeed = 0.1f;

    // 回転
    if (keys[DIK_LEFT])  theta_ -= rotateSpeed;
    if (keys[DIK_RIGHT]) theta_ += rotateSpeed;
    if (keys[DIK_UP])    phi_ -= rotateSpeed;
    if (keys[DIK_DOWN])  phi_ += rotateSpeed;

    phi_ = std::clamp(phi_, -1.5f, 1.5f); // 安定性向上

    // カメラ位置を球面座標で計算
    Vector3 cameraPos = {
        pivot_.x + distance_ * std::cosf(phi_) * std::sinf(theta_),
        pivot_.y + distance_ * std::sinf(phi_),
        pivot_.z + distance_ * std::cosf(phi_) * std::cosf(theta_)
    };

    // forward, right, upベクトルの再計算
    Vector3 worldUp = { 0.0f, 1.0f, 0.0f };
    Vector3 forward = Normalize(pivot_ - cameraPos);
    if (std::abs(Dot(forward, worldUp)) > 0.99f) {
        worldUp = { 0.0f, 0.0f, 1.0f };
    }
    Vector3 right = Normalize(Cross(worldUp, forward));
    Vector3 up = Normalize(Cross(forward, right));

    // 移動（pivotを動かす）
    if (keys[DIK_W]) pivot_ += forward * moveSpeed;
    if (keys[DIK_S]) pivot_ -= forward * moveSpeed;
    if (keys[DIK_A]) pivot_ -= right * moveSpeed;
    if (keys[DIK_D]) pivot_ += right * moveSpeed;
    if (keys[DIK_Q]) pivot_ += up * moveSpeed;
    if (keys[DIK_E]) pivot_ -= up * moveSpeed;

    // カメラ再計算
    cameraPos = {
        pivot_.x + distance_ * std::cosf(phi_) * std::sinf(theta_),
        pivot_.y + distance_ * std::sinf(phi_),
        pivot_.z + distance_ * std::cosf(phi_) * std::cosf(theta_)
    };

    // ビュー・プロジェクション行列
    viewMatrix_ = MakeLookAtMatrix(cameraPos, pivot_, { 0.0f, 1.0f, 0.0f });
    projectionMatrix_ = MakePerspectiveFovMatrix(0.45f, 1280.0f / 720.0f, 0.1f, 100.0f);
}
