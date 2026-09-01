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
    // 選択物の保存先と、生成候補としてUIへ表示するプレハブ一覧。
    std::array<const char*, 3> prefabFilePaths_{
        "resources/prefab_00.json",
        "resources/prefab_01.json",
        "resources/prefab_02.json"
    };
    int sceneFileIndex_ = 0;               // 選択中のシーンファイル番号
    int prefabFileIndex_ = 0;              // 選択中のプレハブファイル番号
    int selectedObjectIndex_ = 0;          // オブジェクト一覧で選択中の番号
    int addModelIndex_ = 2;                // 新規追加に使うモデル一覧番号
    int gizmoMode_ = 0;                    // 移動・回転・拡縮ギズモの選択番号
    EditorMode mode_ = EditorMode::Edit;   // 現在の編集・実行モード
    bool isEditorGuiVisible_ = true;        // エディターパネルを表示するか
    static bool playOnNextEditorOpen_;      // 次回起動直後にPlayへ入る一回限りの要求
    bool requestStartPlayMode_ = false;     // UIから届いたPlay開始要求
    bool requestStopPlayMode_ = false;      // UIから届いたPlay停止要求
    bool requestAddObject_ = false;         // UIから届いたオブジェクト追加要求
    bool requestRemoveObject_ = false;      // UIから届いた選択物削除要求
    bool requestDuplicateObject_ = false;   // UIから届いた選択物複製要求
    bool requestSavePrefab_ = false;        // UIから届いたプレハブ保存要求
    bool requestInstantiatePrefab_ = false; // UIから届いたプレハブ生成要求
    bool requestSaveObjects_ = false;       // UIから届いたシーン保存要求
    bool requestLoadObjects_ = false;       // UIから届いたシーン読込要求
    bool requestUndo_ = false;              // UIから届いた操作取消要求
    bool requestRedo_ = false;              // UIから届いた操作再実行要求
};
