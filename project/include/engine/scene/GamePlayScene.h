#pragma once
#include "engine/scene/BaseScene.h"
#include <array>
#include <memory>
#include <string>
#include <vector>
#include "engine/base/Math.h"
#include "engine/2d/Sprite.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/base/ImGuiManager.h"
#include "engine/editor/EditorManager.h"
#include "engine/scene/SceneSerializer.h"

class Object3dCommon;
class Object3d;
class Camera;
class ParticleEmitter;
class Skybox;

struct SkeletonDebugSet {
    Object3d* source = nullptr;
    std::vector<std::unique_ptr<Object3d>> joints;
    std::vector<std::unique_ptr<Object3d>> bones;
    Math::Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
};

// エディタ上で扱う追加オブジェクト
struct EditorObject {
    std::string name;
    std::unique_ptr<Object3d> object;
    int modelIndex = 0;
};

class GamePlayScene : public BaseScene {
public:
    GamePlayScene();
    ~GamePlayScene() override;

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;

    int GetPostEffectMode() const { return postEffectMode_; }

private:
    struct TransformHistoryRecord {
        Object3d* object = nullptr;
        Math::Vector3 beforeTranslate{};
        Math::Vector3 beforeRotate{};
        Math::Vector3 beforeScale{ 1.0f, 1.0f, 1.0f };
        Math::Vector3 afterTranslate{};
        Math::Vector3 afterRotate{};
        Math::Vector3 afterScale{ 1.0f, 1.0f, 1.0f };
    };

    void UpdateDebugCamera(float deltaTime);
    void InitializeSkeletonDebugSet(
        SkeletonDebugSet& debugSet,
        Object3d* source,
        const Math::Vector4& color);
    void UpdateSkeletonDebugSet(SkeletonDebugSet& debugSet);
    bool LoadPrimitiveObjectsFromSceneFile(const char* path);
    bool SaveSceneToFile(const char* path) const;
    void AddEditorPrimitive(int modelIndex);
    void DuplicateSelectedEditorObject();
    void SaveSelectedEditorObjectAsPrefab();
    void InstantiatePrefab();
    void RemoveSelectedEditorObject();
    void BuildInspectableObjects(
        std::vector<ImGuiManager::InspectableObject>& inspectObjects);
    void ProcessEditorRequests();
    void ApplyLightingToObject(Object3d* object);
    void UpdateAnimations(float deltaTime);
    void LoadEditorSettings();
    void SaveEditorSettings() const;
    std::vector<SceneSerializer::ObjectRecord> BuildSceneObjectRecords() const;
    SceneSerializer::SceneSettings BuildSceneSettings() const;
    void ApplySceneSettings(const SceneSerializer::SceneSettings& settings);
    void TrackTransformHistory(
        const std::vector<ImGuiManager::InspectableObject>& inspectObjects);
    void ClearTransformHistory();
    void ApplyTransformHistory(const TransformHistoryRecord& record, bool useAfter);

    std::unique_ptr<Object3dCommon> object3dCommon_;
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<Skybox> skybox_;
    std::unique_ptr<Object3d> object3d_;
    std::unique_ptr<Object3d> ringObject_;
    std::unique_ptr<Object3d> cylinderObject_;
    std::unique_ptr<Object3d> sphereObject_;
    std::unique_ptr<Object3d> animatedCubeObject_;
    std::unique_ptr<Object3d> simpleSkinObject_;
    std::unique_ptr<Object3d> humanSneakObject_;
    std::unique_ptr<Object3d> humanWalkObject_;
    std::vector<EditorObject> editorObjects_;
    SkeletonDebugSet simpleSkinDebug_;
    SkeletonDebugSet humanSneakDebug_;
    SkeletonDebugSet humanWalkDebug_;
    std::unique_ptr<ParticleEmitter> emitter_;

    std::vector<Sprite> sprites_;
    Math::Vector2 spritePos_{ 100.0f, 100.0f };
    Math::Vector3 objectRotate_{ 0.0f, 0.0f, 0.0f };
    Math::Vector3 lightDirection_{ 0.0f, -1.0f, 0.0f };
    float lightIntensity_ = 1.0f;
    int blendModeIndex_ = static_cast<int>(BlendMode::Normal);
    float environmentCoefficient_ = 0.2f;
    Math::Vector3 pointLightPosition_{ 0.0f, 2.0f, 0.0f };
    float pointLightIntensity_ = 1.0f;
    Math::Vector3 spotLightPosition_{ 2.0f, 1.25f, 0.0f };
    Math::Vector3 spotLightDirection_{ -1.0f, 1.0f, 0.0f };
    float spotLightIntensity_ = 4.0f;
    bool showSkybox_ = false;
    int postEffectMode_ = 0;
    bool showPlane_ = true;
    bool showRing_ = true;
    bool showCylinder_ = true;
    bool showSphere_ = true;
    bool showParticle_ = true;
    Math::Vector4 cylinderColor_{ 0.45f, 0.55f, 1.0f, 0.8f };
    float cylinderAlphaReference_ = 0.01f;
    float cylinderUVScrollSpeed_ = 0.35f;
    float cylinderUVOffset_ = 0.0f;
    float animationTime_ = 0.0f;
    bool showSkinningSamples_ = true;
    bool showSkeletonDebug_ = true;
    bool renderingPanelOpen_ = true;
    bool objectsPanelOpen_ = true;
    bool inspectorPanelOpen_ = true;
    bool materialPanelOpen_ = true;
    bool cylinderPanelOpen_ = false;
    bool lightingPanelOpen_ = true;
    std::vector<TransformHistoryRecord> undoStack_;
    std::vector<TransformHistoryRecord> redoStack_;
    int lastTransformHistoryObjectIndex_ = -1;
    Math::Vector3 lastObservedTranslate_{};
    Math::Vector3 lastObservedRotate_{};
    Math::Vector3 lastObservedScale_{ 1.0f, 1.0f, 1.0f };
    std::array<int, 8> inspectObjectModelIndices_{
        0, 1, 2, 3, 9, 10, 11, 12
    };
    EditorManager editorManager_;

    bool isDebugCameraEnabled_ = false;
    float debugCameraMoveSpeed_ = 6.0f;
    float debugCameraRotateSpeed_ = 1.8f;
};
