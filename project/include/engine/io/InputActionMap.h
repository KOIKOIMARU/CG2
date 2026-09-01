#pragma once

#include "engine/io/Input.h"

#include <array>
#include <cstddef>
#include <cstdint>

// ゲーム内の操作名と物理キーを分離する固定長の入力マッピング。
// Bindは初期化時に行い、更新中はIsPressed / IsTriggeredだけを呼ぶ。
class InputActionMap {
public:
    using ActionId = uint32_t;

    static constexpr size_t kMaxBindingCount = 32;

    // Inputは借用。secondaryKeyを0にすると第2キーを使用しない。
    void Initialize(Input* input);
    bool Bind(ActionId action, BYTE primaryKey, BYTE secondaryKey = 0);
    void Clear();

    bool IsPressed(ActionId action) const;
    bool IsTriggered(ActionId action) const;

private:
    struct Binding {
        ActionId action = 0;       // ゲーム側で定義した操作番号
        BYTE primaryKey = 0;       // 操作に割り当てる第1キー
        BYTE secondaryKey = 0;     // 任意の第2キー。0なら未使用
        bool isActive = false;     // この配列要素に割当てが存在するか
    };

    const Binding* FindBinding(ActionId action) const;
    Binding* FindBinding(ActionId action);

    Input* input_ = nullptr; // 物理キー状態を問い合わせる借用先
    std::array<Binding, kMaxBindingCount> bindings_{}; // 固定長の操作割当て表
};
