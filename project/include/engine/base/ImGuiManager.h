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
// ここは君のプロジェクトの実際のパスに合わせる
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#endif

class ImGuiManager {
public:
    struct RenderingDebugSettings {
        int& blendModeIndex;
        float& environmentCoefficient;
        bool& showSkybox;
        int& postEffectMode;
    };

    struct ObjectDebugSettings {
        Math::Vector3& objectRotate;
        bool& showPlane;
        bool& showRing;
        bool& showCylinder;
        bool& showSphere;
        bool& showParticle;
    };

    struct CylinderEffectDebugSettings {
        Math::Vector4& cylinderColor;
        float& cylinderAlphaReference;
        float& cylinderUVScrollSpeed;
    };

    struct LightingDebugSettings {
        Math::Vector3& lightDirection;
        float& lightIntensity;
        Math::Vector3& pointLightPosition;
        float& pointLightIntensity;
        Math::Vector3& spotLightPosition;
        Math::Vector3& spotLightDirection;
        float& spotLightIntensity;
    };

    struct EditorPanelSettings {
        bool& renderingOpen;
        bool& objectsOpen;
        bool& inspectorOpen;
        bool& materialOpen;
        bool& cylinderOpen;
        bool& lightingOpen;
    };

    enum class InspectableType {
        Object3d,
        DirectionalLight,
        PointLight,
        SpotLight
    };

    struct InspectableObject {
        const char* name;
        InspectableType type = InspectableType::Object3d;
        Object3d* object = nullptr;
        int* selectedModelIndex = nullptr;
        std::string* editableName = nullptr;
    };

    struct ObjectInspectorSettings {
        InspectableObject* objects;
        int objectCount;
        int& selectedObjectIndex;
        int& addModelIndex;
        int& gizmoMode;
        bool& isPlayMode;
        bool& requestAddObject;
        bool& requestRemoveObject;
        bool& requestDuplicateObject;
        bool& requestSavePrefab;
        bool& requestInstantiatePrefab;
        bool& requestSaveObjects;
        bool& requestLoadObjects;
        bool& requestUndo;
        bool& requestRedo;
        const char* saveFilePath;
        const char* const* sceneFileItems;
        int sceneFileItemCount;
        int& sceneFileIndex;
        const char* const* prefabFileItems;
        int prefabFileItemCount;
        int& prefabFileIndex;
    };

    struct GamePlayDebugSettings {
        RenderingDebugSettings rendering;
        ObjectDebugSettings objects;
        CylinderEffectDebugSettings cylinder;
        LightingDebugSettings lighting;
        EditorPanelSettings panels;
        ObjectInspectorSettings inspector;
    };

    void Initialize(WinApp* winApp, DirectXCommon* dxCommon, SrvManager* srvManager);
    void Begin();
    void End();
    void Draw();
    void Finalize();

    void ShowSpriteController(Math::Vector2& spritePos);
    void ShowGamePlayController(GamePlayDebugSettings& settings);

private:
    bool SaveInspectorTransforms(const ObjectInspectorSettings& inspector);
    bool LoadInspectorTransforms(const ObjectInspectorSettings& inspector);

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap_;
    std::string inspectorStatus_;
};
