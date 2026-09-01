#pragma once
#include "engine/scene/BaseScene.h"
#include <array>
#include <memory>
#include <string>
#include <vector>
#include "engine/base/Math.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/base/ImGuiManager.h"
#include "engine/editor/EditorManager.h"
#include "engine/editor/EditorPlayController.h"
#include "engine/scene/SceneSerializer.h"

class Object3dCommon;
class Object3d;
class Camera;
class ParticleEmitter;
class Skybox;

struct SkeletonDebugSet {
    Object3d* source = nullptr;                   // 表示元となるスキニングモデルの借用先
    std::vector<std::unique_ptr<Object3d>> joints;// 各関節位置を示す所有オブジェクト
    std::vector<std::unique_ptr<Object3d>> bones; // 親子関節を結ぶ所有オブジェクト
    Math::Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f }; // デバッグ表示の共通色
};

// エディタ上で扱う1オブジェクト分のデータ。
// Object3d の所有権はこの構造体が持ち、一覧表示用の名前や可視状態もまとめて管理する。
struct EditorObject {
    std::string name;                 // オブジェクト一覧と保存データで使う識別名
    std::unique_ptr<Object3d> object; // EditorSceneが所有する描画オブジェクト
    int modelIndex = 0;               // エディターのモデル一覧番号
    bool visible = true;              // 描画対象に含めるか
    bool runtimePreview = false;      // 保存対象外のエンジン機能見本ならtrue
};

// シーン編集UIと、注入されたゲームランタイムのPlay実行を提供する。
class EditorScene : public BaseScene {
public:
    EditorScene();
    ~EditorScene() override;

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;

    // プレイモードで起動するゲームランタイムはアプリケーション側から注入する。
    void SetPlayRuntimeFactory(EditorPlayController::RuntimeFactory runtimeFactory);

    int GetPostEffectMode() const;
    const Math::Matrix4x4& GetProjectionMatrix() const;
    // 自動テストがチーム用ランタイムの開始完了を判定するために使用する。
    bool IsPlayModeRunning() const;

private:
    struct TransformHistoryRecord {
        Object3d* object = nullptr;                         // 変更対象の借用ポインタ
        Math::Vector3 beforeTranslate{};                    // 操作前の位置
        Math::Vector3 beforeRotate{};                       // 操作前の回転
        Math::Vector3 beforeScale{ 1.0f, 1.0f, 1.0f };     // 操作前の拡縮
        Math::Vector3 afterTranslate{};                     // 操作後の位置
        Math::Vector3 afterRotate{};                        // 操作後の回転
        Math::Vector3 afterScale{ 1.0f, 1.0f, 1.0f };      // 操作後の拡縮
    };

    void UpdateDebugCamera(float deltaTime);
    void InitializeSkeletonDebugSet(
        SkeletonDebugSet& debugSet,
        Object3d* source,
        const Math::Vector4& color);
    void UpdateSkeletonDebugSet(SkeletonDebugSet& debugSet);
    bool LoadPrimitiveObjectsFromSceneFile(const char* path);
    bool SaveSceneToFile(const char* path) const;
    void AddDefaultPreviewObjects();
    void AddDefaultPreviewObject(
        const char* name,
        const char* modelName,
        const Math::Vector3& translate,
        const Math::Vector3& scale,
        const Math::Vector4& color);
    void AddEditorPrimitive(int modelIndex);
    void DuplicateSelectedEditorObject();
    void SaveSelectedEditorObjectAsPrefab();
    void InstantiatePrefab();
    void RemoveSelectedEditorObject();
    void BuildInspectableObjects(
        std::vector<ImGuiManager::InspectableObject>& inspectObjects);
    void ShowEditorGui(
        std::vector<ImGuiManager::InspectableObject>& inspectObjects);
    void ProcessEditorRequests();
    void ApplyLightingToObject(Object3d* object);
    void UpdateAnimations(float deltaTime);
    void HandleEditorShortcuts();
    void ApplyPlayModeRequests(bool& startedPlayModeThisFrame);
    void EnterPlayMode();
    void ExitPlayMode();
    void LoadEditorSettings();
    void SaveEditorSettings() const;
    std::vector<SceneSerializer::ObjectRecord> BuildSceneObjectRecords() const;
    SceneSerializer::SceneSettings BuildSceneSettings() const;
    void ApplySceneSettings(const SceneSerializer::SceneSettings& settings);
    void TrackTransformHistory(
        const std::vector<ImGuiManager::InspectableObject>& inspectObjects);
    void ClearTransformHistory();
    void ApplyTransformHistory(const TransformHistoryRecord& record, bool useAfter);

    std::unique_ptr<Object3dCommon> object3dCommon_; // エディター3D描画の共有設定
    std::unique_ptr<Camera> camera_;                  // 編集ビューを映すカメラ
    std::unique_ptr<Skybox> skybox_;                  // 編集ビューの背景
    std::unique_ptr<Object3d> ringObject_;            // 組み込み形状の機能見本
    std::unique_ptr<Object3d> cylinderObject_;        // 組み込み形状の機能見本
    std::unique_ptr<Object3d> sphereObject_;          // 組み込み形状の機能見本
    std::unique_ptr<Object3d> animatedCubeObject_;    // ノードアニメーションの機能見本
    std::unique_ptr<Object3d> simpleSkinObject_;      // 基本スキニングの機能見本
    std::unique_ptr<Object3d> humanSneakObject_;      // 人型スキニングの機能見本
    std::unique_ptr<Object3d> humanWalkObject_;       // 人型スキニングの機能見本
    EditorPlayController playController_;             // Play用ランタイムの寿命管理
    std::vector<EditorObject> editorObjects_;          // ユーザーが編集する全オブジェクト
    SkeletonDebugSet simpleSkinDebug_;                 // simpleSkinObject_の骨表示
    SkeletonDebugSet humanSneakDebug_;                 // humanSneakObject_の骨表示
    SkeletonDebugSet humanWalkDebug_;                  // humanWalkObject_の骨表示
    std::unique_ptr<ParticleEmitter> emitter_;          // GPUパーティクル機能見本の発生器

    Math::Vector3 lightDirection_{ 0.0f, -1.0f, 0.0f }; // 共通平行光源の向き
    float lightIntensity_ = 1.0f;                       // 共通平行光源の光量
    int blendModeIndex_ = static_cast<int>(BlendMode::Normal); // UIで選んだ色合成方式
    float environmentCoefficient_ = 0.2f;               // 環境マップ反射の共通強度
    Math::Vector3 pointLightPosition_{ 0.0f, 2.0f, 0.0f }; // 共通点光源の位置
    float pointLightIntensity_ = 1.0f;                   // 共通点光源の光量
    Math::Vector3 spotLightPosition_{ 2.0f, 1.25f, 0.0f }; // 共通スポットライトの位置
    Math::Vector3 spotLightDirection_{ -1.0f, 1.0f, 0.0f }; // 共通スポットライトの向き
    float spotLightIntensity_ = 4.0f;                    // 共通スポットライトの光量
    bool showSkybox_ = true;                             // 背景を描画するか
    int postEffectMode_ = 12;                            // 編集ビューのポストエフェクト番号
    float animationTime_ = 0.0f;                         // 機能見本アニメーションの再生秒数
    bool showSkinningSamples_ = true;                    // スキニング見本を描画するか
    bool showSkeletonDebug_ = true;                      // ボーン形状を重ねて描画するか
    bool renderingPanelOpen_ = true;                     // 描画設定パネルの開閉状態
    bool objectsPanelOpen_ = true;                       // オブジェクト一覧パネルの開閉状態
    bool inspectorPanelOpen_ = true;                     // 変換編集パネルの開閉状態
    bool materialPanelOpen_ = true;                      // マテリアルパネルの開閉状態
    bool lightingPanelOpen_ = true;                      // ライトパネルの開閉状態
    bool viewportPanelOpen_ = true;                      // ゲームビューの開閉状態
    bool wasPlayMode_ = false;                           // 前フレームがPlayだったか
    std::vector<TransformHistoryRecord> undoStack_;      // 取消可能な変換履歴
    std::vector<TransformHistoryRecord> redoStack_;      // 再実行可能な変換履歴
    int lastTransformHistoryObjectIndex_ = -1;           // 最後に監視した選択物番号
    Math::Vector3 lastObservedTranslate_{};               // 履歴比較用の直前位置
    Math::Vector3 lastObservedRotate_{};                  // 履歴比較用の直前回転
    Math::Vector3 lastObservedScale_{ 1.0f, 1.0f, 1.0f };// 履歴比較用の直前拡縮
    EditorManager editorManager_;                         // UI選択状態と操作要求

    bool isDebugCameraEnabled_ = false; // キーボードで編集カメラを動かすか
    float debugCameraMoveSpeed_ = 6.0f; // デバッグカメラの移動速度
    float debugCameraRotateSpeed_ = 1.8f; // デバッグカメラの回転速度
};
