#pragma once
#include <array>
#include "engine/base/ImGuiManager.h"

// エディタUIの選択状態と、UIから発生した一回限りの操作要求を保持する。
// シーンやObject3d自体は所有せず、EditorSceneが実処理を担当する。
class EditorManager {
public:
    // Editはシーン編集、Playは注入されたゲームランタイムの実行状態。
    enum class EditorMode {
        Edit,
        Play
    };

    void SetSceneFilePath(const char* path);
    const char* GetSceneFilePath() const;
    const char* GetPrefabFilePath() const;
    int GetSceneFileIndex() const;
    int GetPrefabFileIndex() const;
    void SetSceneFileIndex(int index);
    void SetPrefabFileIndex(int index);
    // ImGuiへ渡す設定を現在状態から組み立てる。objectsの所有権は移動しない。
    ImGuiManager::ObjectInspectorSettings CreateInspectorSettings(
        ImGuiManager::InspectableObject* objects,
        int objectCount);

    void ValidateSelectedObjectIndex(int objectCount);
    int GetSelectedObjectIndex() const;
    void ResetSelectedObjectIndex();
    bool IsPlayMode() const;
    void SetPlayMode(bool isPlayMode);
    EditorMode GetMode() const;
    void SetMode(EditorMode mode);
    bool IsEditorGuiVisible() const;
    void SetEditorGuiVisible(bool isVisible);
    void ToggleEditorGuiVisible();
    // 別シーンからエディタへ戻った直後にプレイを開始するための一回限りの要求。
    static void RequestPlayOnNextEditorOpen();
    static bool ConsumePlayOnNextEditorOpenRequest();

    int GetAddModelIndex() const;
    int GetGizmoMode() const;
    void SetGizmoMode(int mode);

    // Consume系は要求を返すと同時にフラグを消費し、同じ操作の二重実行を防ぐ。
    bool ConsumeAddObjectRequest();
    bool ConsumeStartPlayModeRequest();
    bool ConsumeStopPlayModeRequest();
    bool ConsumeRemoveObjectRequest();
    bool ConsumeDuplicateObjectRequest();
    bool ConsumeSavePrefabRequest();
    bool ConsumeInstantiatePrefabRequest();
    bool ConsumeSaveObjectsRequest();
    bool ConsumeLoadObjectsRequest();
    bool ConsumeUndoRequest();
    bool ConsumeRedoRequest();

private:
    // 先頭要素はチーム用の既定シーン。UIのコンボボックスから切り替える。
    std::array<const char*, 3> sceneFilePaths_{
        "resources/editor_default_scene.json",
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
    EditorMode mode_ = EditorMode::Edit;
    bool isEditorGuiVisible_ = true;
    static bool playOnNextEditorOpen_;
    bool requestStartPlayMode_ = false;
    bool requestStopPlayMode_ = false;
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
