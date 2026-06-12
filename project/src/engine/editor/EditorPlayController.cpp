#include "engine/editor/EditorPlayController.h"

#include "app/GameRuntime.h"

EditorPlayController::EditorPlayController() = default;
EditorPlayController::~EditorPlayController() = default;

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
    if (runtime_) {
        return false;
    }

    runtime_ = std::make_unique<GameRuntime>();
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

    runtime_->SetDebugGuiAllowed(isEditorGuiVisible);
    runtime_->SetRenderingOptions(showSkybox, postEffectMode);
    runtime_->Update();
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

bool EditorPlayController::IsRunning() const
{
    return runtime_ != nullptr;
}

bool EditorPlayController::IsExitRequested() const
{
    return runtime_ && runtime_->IsExitRequested();
}
