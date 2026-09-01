#pragma once
#include "engine/base/Math.h"

// カメラが使用する投影方式。Perspectiveは奥行き表現、Orthographicは平行投影に使う。
enum class CameraProjectionMode {
    Perspective,
    Orthographic,
};

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
    void SetProjectionMode(CameraProjectionMode mode);
    // 正投影で画面に収める縦方向のワールドサイズ。0より大きい値を指定する。
    void SetOrthographicHeight(float height);

    // Updateで最後に計算された行列と姿勢を返す。
    const Math::Matrix4x4& GetWorldMatrix() const;
    const Math::Matrix4x4& GetViewMatrix() const;
    const Math::Matrix4x4& GetProjectionMatrix() const;
    const Math::Matrix4x4& GetViewProjectionMatrix() const;
    const Math::Vector3& GetRotate() const;
    const Math::Vector3& GetTranslate() const;
    CameraProjectionMode GetProjectionMode() const;
    float GetOrthographicHeight() const;

private:
    Math::Transform transform;              // カメラの拡縮・回転・ワールド位置
    Math::Matrix4x4 worldMatrix;            // カメラ自身のワールド行列
    Math::Matrix4x4 viewMatrix;             // ワールド空間からカメラ空間への変換
    Math::Matrix4x4 projectionMatrix;       // 現在の方式に応じた投影変換
    Math::Matrix4x4 viewProjectionMatrix;   // 描画側へ渡すビュー・投影合成行列
    float fovY;                             // 透視投影時の垂直視野角（ラジアン）
    float aspectRatio;                      // 描画領域の横幅と縦幅の比率
    float nearClip;                         // 手前のクリッピング距離
    float farClip;                          // 奥のクリッピング距離
    CameraProjectionMode projectionMode = CameraProjectionMode::Perspective; // 現在の投影方式
    float orthographicHeight = 10.0f;       // 平行投影で画面に収める縦方向の範囲
};
