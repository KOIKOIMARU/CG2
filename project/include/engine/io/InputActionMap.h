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
        ActionId action = 0;
        BYTE primaryKey = 0;
        BYTE secondaryKey = 0;
        bool isActive = false;
    };

    const Binding* FindBinding(ActionId action) const;
    Binding* FindBinding(ActionId action);

    Input* input_ = nullptr;
    std::array<Binding, kMaxBindingCount> bindings_{};
};
