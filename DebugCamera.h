
#pragma once
#include "MathTypes.h"
#include <cstdint>


class DebugCamera {
public:
    void Initialize();
    void Update(const uint8_t* keys);

    // クラス内の private に追加
    Vector3 position_ = {}; // 初期化は Update() で正しく行う


    // GetPosition() の中身を以下のように修正
    Vector3 GetPosition() const {
        return position_;
    }

    const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }
    const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }

private:
    float theta_ = 0.0f;     // 水平角度（Y軸回転）
    float phi_ = 0.0f;       // 垂直角度（X軸回転）
    float distance_ = 5.0f;  // ピボットからの距離

    Vector3 pivot_ = { 0.0f, 0.0f, 0.0f }; // ピボット中心
    Matrix4x4 viewMatrix_;
    Matrix4x4 projectionMatrix_;
};
