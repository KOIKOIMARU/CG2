#pragma once

#include <string>
#include "engine/base/Math.h"

// 一定間隔で指定グループのパーティクルを発生させるタイマー。
class ParticleEmitter
{
public:
    // groupNameはParticleManagerに事前登録したグループ名を指定する。
    ParticleEmitter(
        const std::string& groupName, // ParticleGroup 名
        const Math::Vector3& position,
        float emitInterval,           // 発生間隔（秒）
        uint32_t emitCount             // 1回の発生数
    );

    // 経過時間を進め、発生間隔に達した分だけEmitする。
    void Update(float deltaTime);

    // 現在位置へ設定数のパーティクルを即時発生させる。
    void Emit();

    // 次回の発生位置を変更する。
    void SetPosition(const Math::Vector3& pos) { position_ = pos; }

private:
    std::string groupName_;       // ParticleManagerへ渡す発生グループ名
    Math::Vector3 position_;      // ワールド空間上の発生中心
    float emitInterval_ = 0.1f;   // 自動発生の間隔（秒）
    uint32_t emitCount_ = 1;      // 1回に発生させる粒数
    float elapsedTime_ = 0.0f;    // 前回の自動発生からの経過秒数
};
