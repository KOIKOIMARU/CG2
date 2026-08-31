#pragma once
#include <cmath>

// エンジン内で使用する行ベクトル・左手座標系の数学型と基本演算。
// 行列の平行移動成分はm[3][0..2]に格納し、角度の単位はラジアンとする。
namespace Math {

    // GPU定数バッファとも共有するため、単純なfloat集合として定義する。
    struct Vector2 {
        float x, y;
    };

    struct Vector3 {
        float x, y, z;
    };

    struct Vector4 {
        float x, y, z, w;
    };

    struct Quaternion {
        float x, y, z, w;
    };

    struct Matrix4x4 {
        float m[4][4];
    };

    struct Transform {
        Vector3 scale;
        Vector3 rotate;
        Vector3 translate;
    };

    struct QuaternionTransform {
        Vector3 scale;
        Quaternion rotate;
        Vector3 translate;
    };

    inline Math::Vector3 operator*(const Math::Vector3& v, float s)
    {
        return { v.x * s, v.y * s, v.z * s };
    }

    inline Math::Vector3 operator*(float s, const Math::Vector3& v)
    {
        return { v.x * s, v.y * s, v.z * s };
    }

    inline Math::Vector3& operator+=(Math::Vector3& a, const Math::Vector3& b)
    {
        a.x += b.x;
        a.y += b.y;
        a.z += b.z;
        return a;
    }



    // 基本的な行列生成・合成。Multiply(m1, m2)はm1の後にm2を適用する。
    inline Matrix4x4 MakeIdentity4x4() {
        Matrix4x4 result{};
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                result.m[i][j] = (i == j) ? 1.0f : 0.0f;
            }
        }
        return result;
    }

    inline Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2) {
        Matrix4x4 result{};
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                float v = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    v += m1.m[i][k] * m2.m[k][j];
                }
                result.m[i][j] = v;
            }
        }
        return result;
    }

    inline Matrix4x4 MakeScaleMatrix(const Vector3& scale) {
        Matrix4x4 result{};
        result.m[0][0] = scale.x;
        result.m[1][1] = scale.y;
        result.m[2][2] = scale.z;
        result.m[3][3] = 1.0f;
        return result;
    }

    inline Matrix4x4 MakeTranslateMatrix(const Vector3& translate) {
        Matrix4x4 result{};
        result.m[0][0] = 1.0f;
        result.m[1][1] = 1.0f;
        result.m[2][2] = 1.0f;
        result.m[3][3] = 1.0f;
        result.m[3][0] = translate.x;
        result.m[3][1] = translate.y;
        result.m[3][2] = translate.z;
        return result;
    }

    inline Matrix4x4 MakeRotateXMatrix(float angle) {
        Matrix4x4 result{};
        result.m[0][0] = 1.0f;
        result.m[3][3] = 1.0f;
        float c = std::cos(angle);
        float s = std::sin(angle);
        result.m[1][1] = c;
        result.m[1][2] = s;
        result.m[2][1] = -s;
        result.m[2][2] = c;
        return result;
    }

    inline Matrix4x4 MakeRotateYMatrix(float angle) {
        Matrix4x4 result{};
        result.m[1][1] = 1.0f;
        result.m[3][3] = 1.0f;
        float c = std::cos(angle);
        float s = std::sin(angle);
        result.m[0][0] = c;
        result.m[0][2] = -s;
        result.m[2][0] = s;
        result.m[2][2] = c;
        return result;
    }

    inline Matrix4x4 MakeRotateZMatrix(float angle) {
        Matrix4x4 result{};
        result.m[2][2] = 1.0f;
        result.m[3][3] = 1.0f;
        float c = std::cos(angle);
        float s = std::sin(angle);
        result.m[0][0] = c;
        result.m[0][1] = s;
        result.m[1][0] = -s;
        result.m[1][1] = c;
        return result;
    }

    inline Matrix4x4 MakeRotateMatrix(const Quaternion& q)
    {
        Matrix4x4 result = MakeIdentity4x4();

        const float xx = q.x * q.x;
        const float yy = q.y * q.y;
        const float zz = q.z * q.z;
        const float xy = q.x * q.y;
        const float xz = q.x * q.z;
        const float yz = q.y * q.z;
        const float wx = q.w * q.x;
        const float wy = q.w * q.y;
        const float wz = q.w * q.z;

        result.m[0][0] = 1.0f - 2.0f * (yy + zz);
        result.m[0][1] = 2.0f * (xy + wz);
        result.m[0][2] = 2.0f * (xz - wy);
        result.m[1][0] = 2.0f * (xy - wz);
        result.m[1][1] = 1.0f - 2.0f * (xx + zz);
        result.m[1][2] = 2.0f * (yz + wx);
        result.m[2][0] = 2.0f * (xz + wy);
        result.m[2][1] = 2.0f * (yz - wx);
        result.m[2][2] = 1.0f - 2.0f * (xx + yy);
        return result;
    }

    inline Matrix4x4 MakeAffineMatrix(
        const Vector3& scale,
        const Vector3& rotate,
        const Vector3& translate)
    {
        Matrix4x4 result{};
        Matrix4x4 rotateXYZ =
            Multiply(MakeRotateXMatrix(rotate.x),
                Multiply(MakeRotateYMatrix(rotate.y),
                    MakeRotateZMatrix(rotate.z)));

        result.m[0][0] = scale.x * rotateXYZ.m[0][0];
        result.m[0][1] = scale.x * rotateXYZ.m[0][1];
        result.m[0][2] = scale.x * rotateXYZ.m[0][2];
        result.m[1][0] = scale.y * rotateXYZ.m[1][0];
        result.m[1][1] = scale.y * rotateXYZ.m[1][1];
        result.m[1][2] = scale.y * rotateXYZ.m[1][2];
        result.m[2][0] = scale.z * rotateXYZ.m[2][0];
        result.m[2][1] = scale.z * rotateXYZ.m[2][1];
        result.m[2][2] = scale.z * rotateXYZ.m[2][2];
        result.m[3][0] = translate.x;
        result.m[3][1] = translate.y;
        result.m[3][2] = translate.z;
        result.m[3][3] = 1.0f;

        return result;
    }

    inline Matrix4x4 MakeAffineMatrix(
        const Vector3& scale,
        const Quaternion& rotate,
        const Vector3& translate)
    {
        Matrix4x4 result{};
        Matrix4x4 rotateMatrix = MakeRotateMatrix(rotate);

        result.m[0][0] = scale.x * rotateMatrix.m[0][0];
        result.m[0][1] = scale.x * rotateMatrix.m[0][1];
        result.m[0][2] = scale.x * rotateMatrix.m[0][2];
        result.m[1][0] = scale.y * rotateMatrix.m[1][0];
        result.m[1][1] = scale.y * rotateMatrix.m[1][1];
        result.m[1][2] = scale.y * rotateMatrix.m[1][2];
        result.m[2][0] = scale.z * rotateMatrix.m[2][0];
        result.m[2][1] = scale.z * rotateMatrix.m[2][1];
        result.m[2][2] = scale.z * rotateMatrix.m[2][2];
        result.m[3][0] = translate.x;
        result.m[3][1] = translate.y;
        result.m[3][2] = translate.z;
        result.m[3][3] = 1.0f;

        return result;
    }
    
    inline Matrix4x4 Transpose(const Matrix4x4& m)
    {
        Matrix4x4 result;
        result.m[0][0] = m.m[0][0]; result.m[0][1] = m.m[1][0]; result.m[0][2] = m.m[2][0]; result.m[0][3] = m.m[3][0];
        result.m[1][0] = m.m[0][1]; result.m[1][1] = m.m[1][1]; result.m[1][2] = m.m[2][1]; result.m[1][3] = m.m[3][1];
        result.m[2][0] = m.m[0][2]; result.m[2][1] = m.m[1][2]; result.m[2][2] = m.m[2][2]; result.m[2][3] = m.m[3][2];
        result.m[3][0] = m.m[0][3]; result.m[3][1] = m.m[1][3]; result.m[3][2] = m.m[2][3]; result.m[3][3] = m.m[3][3];
        return result;
    }


    // 長さ0のベクトルはゼロベクトルとして返し、NaNの伝播を防ぐ。
    inline Vector3 Normalize(const Vector3& v) {
        float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        if (len == 0.0f) return { 0.0f, 0.0f, 0.0f };
        return { v.x / len, v.y / len, v.z / len };
    }

    inline Quaternion NormalizeQuaternion(const Quaternion& q) {
        float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
        if (len == 0.0f) {
            return { 0.0f, 0.0f, 0.0f, 1.0f };
        }
        return { q.x / len, q.y / len, q.z / len, q.w / len };
    }

    // DirectXの深度範囲0～1に対応した透視投影行列を生成する。
    inline Matrix4x4 MakePerspectiveFovMatrix(
        float fovY, float aspectRatio, float nearClip, float farClip)
    {
        Matrix4x4 result{};
        float f = 1.0f / std::tan(fovY / 2.0f);
        result.m[0][0] = f / aspectRatio;
        result.m[1][1] = f;
        result.m[2][2] = farClip / (farClip - nearClip);
        result.m[2][3] = 1.0f;
        result.m[3][2] = -(farClip * nearClip) / (farClip - nearClip);
        return result;
    }

    inline Matrix4x4 MakeOrthographicMatrix(
        float left, float top, float right, float bottom,
        float nearClip, float farClip)
    {
        Matrix4x4 result{};
        result.m[0][0] = 2.0f / (right - left);
        result.m[1][1] = 2.0f / (top - bottom);
        result.m[2][2] = 1.0f / (farClip - nearClip);
        result.m[3][0] = (left + right) / (left - right);
        result.m[3][1] = (top + bottom) / (bottom - top);
        result.m[3][2] = -nearClip / (farClip - nearClip);
        result.m[3][3] = 1.0f;
        return result;
    }

    // 逆行列を求める。特異行列を渡さないこと。
    Matrix4x4 Inverse(const Matrix4x4& m);
    // 2つのクォータニオンを最短経路で球面線形補間する。tの想定範囲は0～1。
    Quaternion Slerp(const Quaternion& q0, const Quaternion& q1, float t);

} // namespace Math
