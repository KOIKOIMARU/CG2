#pragma once
#include <cstdint>
#include <string>
#include <wrl.h>
#include <d3d12.h>
#include "engine/base/Math.h"

class WinApp;
class DirectXCommon;
class SrvManager;
class Object3d;

#ifdef USE_IMGUI
// ImGui本体はproject/externals/imguiに統一し、別バージョンとの混在を避ける。
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#endif

// ImGuiのフレーム処理とエディタ用パネルを管理する。
// BeginとEndはUpdate中、Drawは3D描画とポストエフェクトの後に呼ぶ。
class ImGuiManager {
public:
    struct RenderingDebugSettings {
        int& blendModeIndex;              // Object3dの色合成方式番号
        float& environmentCoefficient;    // 環境マップ反射の共通強度
        bool& showSkybox;                  // 背景表示の切替え
        int& postEffectMode;               // 全画面ポストエフェクト番号
    };

    struct LightingDebugSettings {
        Math::Vector3& lightDirection;      // 平行光源が進む方向
        float& lightIntensity;              // 平行光源の光量
        Math::Vector3& pointLightPosition;  // 点光源の位置
        float& pointLightIntensity;         // 点光源の光量
        Math::Vector3& spotLightPosition;   // スポットライトの位置
        Math::Vector3& spotLightDirection;  // スポットライトが進む方向
        float& spotLightIntensity;          // スポットライトの光量
    };

    struct EditorPanelSettings {
        bool& renderingOpen; // 描画設定パネルの開閉状態
        bool& objectsOpen;   // オブジェクト一覧の開閉状態
        bool& inspectorOpen; // 変換編集パネルの開閉状態
        bool& materialOpen;  // マテリアルパネルの開閉状態
        bool& lightingOpen;  // ライトパネルの開閉状態
        bool& viewportOpen;  // ゲームビューの開閉状態
    };

    enum class InspectableType {
        Object3d,
        DirectionalLight,
        PointLight,
        SpotLight
    };

    struct InspectableObject {
        const char* name;                              // 一覧へ表示する名前
        InspectableType type = InspectableType::Object3d; // 編集対象の種類
        Object3d* object = nullptr;                    // 3Dオブジェクトの借用先
        int* selectedModelIndex = nullptr;             // モデル選択番号の編集先
        std::string* editableName = nullptr;           // 名前の編集先
        bool* visible = nullptr;                       // 表示フラグの編集先
        bool readOnly = false;                         // trueなら値の変更を禁止する
    };

    struct ObjectInspectorSettings {
        InspectableObject* objects;           // 表示対象配列の先頭
        int objectCount;                      // objectsの有効要素数
        int& selectedObjectIndex;             // 一覧で選択中の番号
        int& addModelIndex;                    // 新規追加に使うモデル番号
        int& gizmoMode;                        // 移動・回転・拡縮ギズモの番号
        bool isPlayMode;                       // 現在Play中か
        bool& requestStartPlayMode;            // Play開始要求の出力先
        bool& requestStopPlayMode;             // Play停止要求の出力先
        bool& requestAddObject;                // 追加要求の出力先
        bool& requestRemoveObject;             // 削除要求の出力先
        bool& requestDuplicateObject;          // 複製要求の出力先
        bool& requestSavePrefab;               // プレハブ保存要求の出力先
        bool& requestInstantiatePrefab;        // プレハブ生成要求の出力先
        bool& requestSaveObjects;              // シーン保存要求の出力先
        bool& requestLoadObjects;              // シーン読込要求の出力先
        bool& requestUndo;                     // 取消要求の出力先
        bool& requestRedo;                     // 再実行要求の出力先
        const char* saveFilePath;               // 現在のシーン保存先
        const char* const* sceneFileItems;      // シーンファイル選択肢
        int sceneFileItemCount;                 // シーン選択肢の数
        int& sceneFileIndex;                    // 選択中のシーン番号
        const char* const* prefabFileItems;     // プレハブファイル選択肢
        int prefabFileItemCount;                // プレハブ選択肢の数
        int& prefabFileIndex;                   // 選択中のプレハブ番号
    };

    struct EditorDebugSettings {
        RenderingDebugSettings rendering;
        LightingDebugSettings lighting;
        EditorPanelSettings panels;
        ObjectInspectorSettings inspector;
    };

    void Initialize(WinApp* winApp, DirectXCommon* dxCommon, SrvManager* srvManager);
    void Begin();
    void End();
    void Draw();
    void Finalize();

    void ShowEditorController(EditorDebugSettings& settings);
    void ShowViewport(
        D3D12_GPU_DESCRIPTOR_HANDLE textureHandle,
        const Math::Vector2& textureSize,
        const Math::Matrix4x4& viewMatrix,
        const Math::Matrix4x4& projectionMatrix,
        bool& isOpen,
        bool isPlayMode,
        bool& requestStartPlayMode,
        bool& requestStopPlayMode,
        ObjectInspectorSettings& inspector);
    bool GetLastViewportImageRect(Math::Vector2& min, Math::Vector2& size) const;

private:
    bool SaveInspectorTransforms(const ObjectInspectorSettings& inspector);
    bool LoadInspectorTransforms(const ObjectInspectorSettings& inspector);

    DirectXCommon* dxCommon_ = nullptr;       // ImGui描画を記録するDirectX基盤の借用先
    SrvManager* srvManager_ = nullptr;        // ビューポート画像SRVの参照先
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap_; // ImGui専用フォントSRVヒープ
    std::string inspectorStatus_;             // 保存・読込結果をUIへ表示する短いメッセージ
    Math::Vector2 lastViewportImageMin_{ 0.0f, 0.0f }; // 最後に描画したゲーム画像の左上座標
    Math::Vector2 lastViewportImageSize_{ 0.0f, 0.0f };// 最後に描画したゲーム画像の大きさ
    bool hasLastViewportImageRect_ = false;    // 上記の矩形が今フレーム利用可能か
};
