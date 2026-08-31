#include "engine/editor/EditorPlayController.h"

#include <utility>

EditorPlayController::EditorPlayController() = default;
EditorPlayController::~EditorPlayController() = default;

void EditorPlayController::SetRuntimeFactory(RuntimeFactory runtimeFactory)
{
    // 具体的なゲーム型をengine層へ持ち込まず、生成処理だけをapp層から受け取る。
    runtimeFactory_ = std::move(runtimeFactory);
}

void EditorPlayController::SetSystems(
    DirectXCommon* dxCommon,
    SrvManager* srvManager,
    SpriteCommon* spriteCommon,
    ImGuiManager* imguiManager,
    Input* input)
{
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    spriteCommon_ = spriteCommon;
    imguiManager_ = imguiManager;
    input_ = input;
}

bool EditorPlayController::Start()
{
    // 多重起動と未設定ファクトリを拒否し、ランタイムを常に一つだけ所有する。
    if (runtime_ || !runtimeFactory_) {
        return false;
    }

    runtime_ = runtimeFactory_();
    if (!runtime_) {
        return false;
    }
    runtime_->SetSystems(
        dxCommon_,
        srvManager_,
        spriteCommon_,
        imguiManager_,
        input_);
    runtime_->Initialize();
    return true;
}

void EditorPlayController::Stop()
{
    if (!runtime_) {
        return;
    }

    // GPU資源などを明示的に解放してから所有権を破棄する。
    runtime_->Finalize();
    runtime_.reset();
}

void EditorPlayController::Update(
    bool isEditorGuiVisible,
    bool showSkybox,
    int postEffectMode)
{
    if (!runtime_) {
        return;
    }

    (void)isEditorGuiVisible;
    runtime_->SetRenderingOptions(showSkybox, postEffectMode);
    runtime_->Update();
}

void EditorPlayController::SetHudViewportRect(
    bool isEnabled,
    const Math::Vector2& min,
    const Math::Vector2& size)
{
    if (runtime_) {
        runtime_->SetHudViewportRect(isEnabled, min, size);
    }
}

void EditorPlayController::Draw()
{
    if (runtime_) {
        runtime_->Draw();
    }
}

int EditorPlayController::GetPostEffectMode() const
{
    return runtime_ ? runtime_->GetPostEffectMode() : 0;
}

const Math::Matrix4x4* EditorPlayController::GetProjectionMatrix() const
{
    return runtime_ ? &runtime_->GetProjectionMatrix() : nullptr;
}

bool EditorPlayController::IsRunning() const
{
    return runtime_ != nullptr;
}

bool EditorPlayController::IsExitRequested() const
{
    return runtime_ && runtime_->IsExitRequested();
}
