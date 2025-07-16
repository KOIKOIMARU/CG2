
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

void DebugCamera::Initialize(const Vector3& pos, const Vector3& rotation) {
    position_ = pos;
    yaw_ = rotation.y;
    pitch_ = rotation.x;
}

void DebugCamera::Update(const uint8_t* keys) {
    const float moveSpeed = 0.1f;
    const float rotateSpeed = 0.02f;

    // ★ nullチェックを追加
    if (keys) {
        if (keys[DIK_LEFT]) yaw_ -= rotateSpeed;
        if (keys[DIK_RIGHT]) yaw_ += rotateSpeed;
        if (keys[DIK_UP]) pitch_ += rotateSpeed;
        if (keys[DIK_DOWN]) pitch_ -= rotateSpeed;
    }
    // 上下回転の制限
    pitch_ = std::clamp(pitch_, -1.5f, 1.5f);

    // 回転から方向ベクトルを算出（Y軸回転 → X軸回転）
    Vector3 forward = {
        cosf(pitch_) * sinf(yaw_),
        sinf(pitch_),
        cosf(pitch_) * cosf(yaw_)
    };
    forward = Normalize(forward);

    Vector3 right = Normalize(Cross({ 0,1,0 }, forward));
    Vector3 up = Normalize(Cross(forward, right));

    // キーで移動（WASD）
    if (keys) {
        if (keys[DIK_W]) position_ += forward * moveSpeed;
        if (keys[DIK_S]) position_ -= forward * moveSpeed;
        if (keys[DIK_A]) position_ -= right * moveSpeed;
        if (keys[DIK_D]) position_ += right * moveSpeed;
        if (keys[DIK_Q]) position_ += up * moveSpeed;
        if (keys[DIK_E]) position_ -= up * moveSpeed;
    }

    // ビュー行列生成
    viewMatrix_ = MakeLookLhMatrix(position_, position_ + forward, { 0, 1, 0 });
    projectionMatrix_ = MakePerspectiveFovMatrix(0.45f, 1280.0f / 720.0f, 0.1f, 100.0f);
}


