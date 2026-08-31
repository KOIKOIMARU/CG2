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
        int& blendModeIndex;
        float& environmentCoefficient;
        bool& showSkybox;
        int& postEffectMode;
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
        bool& lightingOpen;
        bool& viewportOpen;
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
        bool* visible = nullptr;
        bool readOnly = false;
    };

    struct ObjectInspectorSettings {
        InspectableObject* objects;
        int objectCount;
        int& selectedObjectIndex;
        int& addModelIndex;
        int& gizmoMode;
        bool isPlayMode;
        bool& requestStartPlayMode;
        bool& requestStopPlayMode;
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

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap_;
    std::string inspectorStatus_;
    Math::Vector2 lastViewportImageMin_{ 0.0f, 0.0f };
    Math::Vector2 lastViewportImageSize_{ 0.0f, 0.0f };
    bool hasLastViewportImageRect_ = false;
};
