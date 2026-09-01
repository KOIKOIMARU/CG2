#include "engine/io/InputActionMap.h"

void InputActionMap::Initialize(Input* input)
{
    input_ = input;
    Clear();
}

bool InputActionMap::Bind(
    ActionId action,
    BYTE primaryKey,
    BYTE secondaryKey)
{
    if (Binding* existing = FindBinding(action)) {
        existing->primaryKey = primaryKey;
        existing->secondaryKey = secondaryKey;
        return true;
    }

    for (Binding& binding : bindings_) {
        if (!binding.isActive) {
            binding.action = action;
            binding.primaryKey = primaryKey;
            binding.secondaryKey = secondaryKey;
            binding.isActive = true;
            return true;
        }
    }
    return false;
}

void InputActionMap::Clear()
{
    bindings_.fill(Binding{});
}

bool InputActionMap::IsPressed(ActionId action) const
{
    const Binding* binding = FindBinding(action);
    if (!input_ || !binding) {
        return false;
    }
    return input_->PushKey(binding->primaryKey) ||
        (binding->secondaryKey != 0 && input_->PushKey(binding->secondaryKey));
}

bool InputActionMap::IsTriggered(ActionId action) const
{
    const Binding* binding = FindBinding(action);
    if (!input_ || !binding) {
        return false;
    }
    return input_->TriggerKey(binding->primaryKey) ||
        (binding->secondaryKey != 0 && input_->TriggerKey(binding->secondaryKey));
}

const InputActionMap::Binding* InputActionMap::FindBinding(ActionId action) const
{
    for (const Binding& binding : bindings_) {
        if (binding.isActive && binding.action == action) {
            return &binding;
        }
    }
    return nullptr;
}

InputActionMap::Binding* InputActionMap::FindBinding(ActionId action)
{
    for (Binding& binding : bindings_) {
        if (binding.isActive && binding.action == action) {
            return &binding;
        }
    }
    return nullptr;
}
