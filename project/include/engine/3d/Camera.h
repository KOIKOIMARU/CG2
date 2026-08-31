#pragma once
#include "engine/base/Math.h"

// 位置・回転と透視投影パラメータから、描画用の各種行列を生成するカメラ。
// 値を変更したフレームではUpdateを呼んでから行列を参照する。
class Camera {
public:
    Camera();
    // 現在のパラメータをWorld、View、Projection、ViewProjection行列へ反映する。
    void Update();

    // 姿勢と透視投影パラメータを設定する。角度はラジアン。
    void SetRotate(const Math::Vector3& rotate);
    void SetTranslate(const Math::Vector3& translate);
    void SetFovY(float fovY);
    void SetAspectRatio(float aspectRatio);
    void SetNearClip(float nearClip);
    void SetFarClip(float farClip);

    // Updateで最後に計算された行列と姿勢を返す。
    const Math::Matrix4x4& GetWorldMatrix() const;
    const Math::Matrix4x4& GetViewMatrix() const;
    const Math::Matrix4x4& GetProjectionMatrix() const;
    const Math::Matrix4x4& GetViewProjectionMatrix() const;
    const Math::Vector3& GetRotate() const;
    const Math::Vector3& GetTranslate() const;

private:
    // カメラのワールド空間上の姿勢。
    Math::Transform transform;

    // Updateで再計算する描画用行列。
    Math::Matrix4x4 worldMatrix;
    Math::Matrix4x4 viewMatrix;
    Math::Matrix4x4 projectionMatrix;
    Math::Matrix4x4 viewProjectionMatrix;

    // 透視投影パラメータ。
    float fovY;
    float aspectRatio;
    float nearClip;
    float farClip;
};
