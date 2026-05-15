#include "engine/editor/EditorManager.h"

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
    return {
        objects,
        objectCount,
        selectedObjectIndex_,
        addModelIndex_,
        gizmoMode_,
        isPlayMode_,
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
    return isPlayMode_;
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
