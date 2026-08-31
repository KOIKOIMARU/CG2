#include "engine/editor/EditorManager.h"

bool EditorManager::playOnNextEditorOpen_ = false;

// EditorManagerはUI状態だけを管理し、ファイル保存やオブジェクト操作そのものは行わない。

void EditorManager::SetSceneFilePath(const char* path)
{
    sceneFilePaths_[0] = path;
}

const char* EditorManager::GetSceneFilePath() const
{
    return sceneFilePaths_[sceneFileIndex_];
}

const char* EditorManager::GetPrefabFilePath() const
{
    return prefabFilePaths_[prefabFileIndex_];
}

int EditorManager::GetSceneFileIndex() const
{
    return sceneFileIndex_;
}

int EditorManager::GetPrefabFileIndex() const
{
    return prefabFileIndex_;
}

void EditorManager::SetSceneFileIndex(int index)
{
    if (0 <= index && index < static_cast<int>(sceneFilePaths_.size())) {
        sceneFileIndex_ = index;
    }
}

void EditorManager::SetPrefabFileIndex(int index)
{
    if (0 <= index && index < static_cast<int>(prefabFilePaths_.size())) {
        prefabFileIndex_ = index;
    }
}

ImGuiManager::ObjectInspectorSettings EditorManager::CreateInspectorSettings(
    ImGuiManager::InspectableObject* objects,
    int objectCount)
{
    // ImGuiが変更した要求フラグは、EditorSceneがConsume系関数で一度だけ処理する。
    return {
        objects,
        objectCount,
        selectedObjectIndex_,
        addModelIndex_,
        gizmoMode_,
        mode_ == EditorMode::Play,
        requestStartPlayMode_,
        requestStopPlayMode_,
        requestAddObject_,
        requestRemoveObject_,
        requestDuplicateObject_,
        requestSavePrefab_,
        requestInstantiatePrefab_,
        requestSaveObjects_,
        requestLoadObjects_,
        requestUndo_,
        requestRedo_,
        GetSceneFilePath(),
        sceneFilePaths_.data(),
        static_cast<int>(sceneFilePaths_.size()),
        sceneFileIndex_,
        prefabFilePaths_.data(),
        static_cast<int>(prefabFilePaths_.size()),
        prefabFileIndex_
    };
}

void EditorManager::ValidateSelectedObjectIndex(int objectCount)
{
    if (selectedObjectIndex_ < 0 || selectedObjectIndex_ >= objectCount) {
        selectedObjectIndex_ = 0;
    }
}

int EditorManager::GetSelectedObjectIndex() const
{
    return selectedObjectIndex_;
}

void EditorManager::ResetSelectedObjectIndex()
{
    selectedObjectIndex_ = 0;
}

bool EditorManager::IsPlayMode() const
{
    return mode_ == EditorMode::Play;
}

void EditorManager::SetPlayMode(bool isPlayMode)
{
    mode_ = isPlayMode ? EditorMode::Play : EditorMode::Edit;
}

EditorManager::EditorMode EditorManager::GetMode() const
{
    return mode_;
}

void EditorManager::SetMode(EditorMode mode)
{
    mode_ = mode;
}

bool EditorManager::IsEditorGuiVisible() const
{
    return isEditorGuiVisible_;
}

void EditorManager::SetEditorGuiVisible(bool isVisible)
{
    isEditorGuiVisible_ = isVisible;
}

void EditorManager::ToggleEditorGuiVisible()
{
    isEditorGuiVisible_ = !isEditorGuiVisible_;
}

void EditorManager::RequestPlayOnNextEditorOpen()
{
    playOnNextEditorOpen_ = true;
}

bool EditorManager::ConsumePlayOnNextEditorOpenRequest()
{
    // 読み取りと解除を同時に行い、画面を開き直した際の誤再生を防ぐ。
    const bool requested = playOnNextEditorOpen_;
    playOnNextEditorOpen_ = false;
    return requested;
}

bool EditorManager::ConsumeStartPlayModeRequest()
{
    const bool requested = requestStartPlayMode_;
    requestStartPlayMode_ = false;
    return requested;
}

bool EditorManager::ConsumeStopPlayModeRequest()
{
    const bool requested = requestStopPlayMode_;
    requestStopPlayMode_ = false;
    return requested;
}

int EditorManager::GetAddModelIndex() const
{
    return addModelIndex_;
}

int EditorManager::GetGizmoMode() const
{
    return gizmoMode_;
}

void EditorManager::SetGizmoMode(int mode)
{
    constexpr int kGizmoModeCount = 3;
    if (0 <= mode && mode < kGizmoModeCount) {
        gizmoMode_ = mode;
    }
}

bool EditorManager::ConsumeAddObjectRequest()
{
    const bool requested = requestAddObject_;
    requestAddObject_ = false;
    return requested;
}

bool EditorManager::ConsumeRemoveObjectRequest()
{
    const bool requested = requestRemoveObject_;
    requestRemoveObject_ = false;
    return requested;
}

bool EditorManager::ConsumeDuplicateObjectRequest()
{
    const bool requested = requestDuplicateObject_;
    requestDuplicateObject_ = false;
    return requested;
}

bool EditorManager::ConsumeSavePrefabRequest()
{
    const bool requested = requestSavePrefab_;
    requestSavePrefab_ = false;
    return requested;
}

bool EditorManager::ConsumeInstantiatePrefabRequest()
{
    const bool requested = requestInstantiatePrefab_;
    requestInstantiatePrefab_ = false;
    return requested;
}

bool EditorManager::ConsumeSaveObjectsRequest()
{
    const bool requested = requestSaveObjects_;
    requestSaveObjects_ = false;
    return requested;
}

bool EditorManager::ConsumeLoadObjectsRequest()
{
    const bool requested = requestLoadObjects_;
    requestLoadObjects_ = false;
    return requested;
}

bool EditorManager::ConsumeUndoRequest()
{
    const bool requested = requestUndo_;
    requestUndo_ = false;
    return requested;
}

bool EditorManager::ConsumeRedoRequest()
{
    const bool requested = requestRedo_;
    requestRedo_ = false;
    return requested;
}
