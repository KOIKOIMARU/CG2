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
    const float zoomSpeed = 0.1f;

    if (keys[DIK_LEFT])  theta_ -= rotateSpeed;
    if (keys[DIK_RIGHT]) theta_ += rotateSpeed;
    if (keys[DIK_UP])    phi_ += rotateSpeed;
    if (keys[DIK_DOWN])  phi_ -= rotateSpeed;

    phi_ = std::clamp(phi_, -1.55f, 1.55f);

    if (keys[DIK_W]) distance_ -= zoomSpeed;
    if (keys[DIK_S]) distance_ += zoomSpeed;

    distance_ = Max(distance_, 0.1f);      

    Vector3 cameraPos = {
        pivot_.x + distance_ * std::cosf(phi_) * std::sinf(theta_),
        pivot_.y + distance_ * std::sinf(phi_),
        pivot_.z + distance_ * std::cosf(phi_) * std::cosf(theta_)
    };

    viewMatrix_ = MakeLookAtMatrix(cameraPos, pivot_, { 0.0f, 1.0f, 0.0f });
    projectionMatrix_ = MakePerspectiveFovMatrix(0.45f, 1280.0f / 720.0f, 0.1f, 100.0f);
}
