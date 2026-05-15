#pragma once
#include <array>
#include "engine/base/ImGuiManager.h"

class EditorManager {
public:
    void SetSceneFilePath(const char* path);
    const char* GetSceneFilePath() const;
    const char* GetPrefabFilePath() const;
    int GetSceneFileIndex() const;
    int GetPrefabFileIndex() const;
    void SetSceneFileIndex(int index);
    void SetPrefabFileIndex(int index);
    ImGuiManager::ObjectInspectorSettings CreateInspectorSettings(
        ImGuiManager::InspectableObject* objects,
        int objectCount);

    void ValidateSelectedObjectIndex(int objectCount);
    int GetSelectedObjectIndex() const;
    void ResetSelectedObjectIndex();
    bool IsPlayMode() const;

    int GetAddModelIndex() const;
    int GetGizmoMode() const;
    void SetGizmoMode(int mode);

    bool ConsumeAddObjectRequest();
    bool ConsumeRemoveObjectRequest();
    bool ConsumeDuplicateObjectRequest();
    bool ConsumeSavePrefabRequest();
    bool ConsumeInstantiatePrefabRequest();
    bool ConsumeSaveObjectsRequest();
    bool ConsumeLoadObjectsRequest();
    bool ConsumeUndoRequest();
    bool ConsumeRedoRequest();

private:
    std::array<const char*, 3> sceneFilePaths_{
        "resources/game_scene.json",
        "resources/scene_01.json",
        "resources/scene_02.json"
    };
    std::array<const char*, 3> prefabFilePaths_{
        "resources/prefab_00.json",
        "resources/prefab_01.json",
        "resources/prefab_02.json"
    };
    int sceneFileIndex_ = 0;
    int prefabFileIndex_ = 0;
    int selectedObjectIndex_ = 0;
    int addModelIndex_ = 2;
    int gizmoMode_ = 0;
    bool isPlayMode_ = false;
    bool requestAddObject_ = false;
    bool requestRemoveObject_ = false;
    bool requestDuplicateObject_ = false;
    bool requestSavePrefab_ = false;
    bool requestInstantiatePrefab_ = false;
    bool requestSaveObjects_ = false;
    bool requestLoadObjects_ = false;
    bool requestUndo_ = false;
    bool requestRedo_ = false;
};
