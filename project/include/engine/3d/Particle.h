#pragma once

#include "engine/base/Math.h"

// CPU側で扱うパーティクル1粒分の状態。
// 描画資源はParticleManagerがグループ単位で所有する。
struct Particle
{
    Math::Vector3 position{};                   // ワールド空間上の位置
    Math::Vector3 scale{ 1.0f, 1.0f, 1.0f };   // 描画サイズ
    Math::Vector3 rotate{};                     // 各軸の回転角（ラジアン）
    Math::Vector3 velocity{};                   // 1秒あたりの移動量
    Math::Vector3 acceleration{};               // 速度へ加算する重力や場の影響
    float lifeTime = 1.0f;                      // 生成から消滅までの秒数
    float currentTime = 0.0f;                   // 生成後の経過秒数

    // 寿命が残っているかを返す。
    bool IsAlive() const
    {
        return currentTime < lifeTime;
    }
};
