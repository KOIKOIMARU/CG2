#include "engine/base/ImGuiManager.h"
#include "engine/base/WinApp.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/SrvManager.h"
#include "engine/3d/Object3d.h"
#include "engine/scene/SceneSerializer.h"

#include <cassert>
#include <cstdio>

namespace {

#ifdef USE_IMGUI

void AllocateImGuiSrvDescriptor(
    ImGui_ImplDX12_InitInfo* info,
    D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle,
    D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle)
{
    auto* srvManager = static_cast<SrvManager*>(info->UserData);
    assert(srvManager);

    uint32_t srvIndex = srvManager->Allocate();
    *outCpuHandle = srvManager->GetCPUDescriptorHandle(srvIndex);
    *outGpuHandle = srvManager->GetGPUDescriptorHandle(srvIndex);
}

void FreeImGuiSrvDescriptor(
    ImGui_ImplDX12_InitInfo* info,
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
    D3D12_GPU_DESCRIPTOR_HANDLE)
{
    auto* srvManager = static_cast<SrvManager*>(info->UserData);
    assert(srvManager);

    // ImGuiが不要になったSRVを、次の確保で再利用できるように戻す
    srvManager->Free(cpuHandle);
}

#endif
}

bool ImGuiManager::SaveInspectorTransforms(
    const ObjectInspectorSettings& inspector)
{
    if (!inspector.saveFilePath) {
        inspectorStatus_ = "Save failed: no path";
        return false;
    }

    std::vector<SceneSerializer::ObjectRecord> records;
    for (int index = 0; index < inspector.objectCount; ++index) {
        Object3d* object = inspector.objects[index].object;
        if (!object) {
            continue;
        }

        SceneSerializer::ObjectRecord record{};
        record.name = inspector.objects[index].name;
        record.primitive = index >= 1;
        record.modelIndex =
            inspector.objects[index].selectedModelIndex ?
            *inspector.objects[index].selectedModelIndex :
            -1;
        record.translate = object->GetTranslate();
        record.rotate = object->GetRotate();
        record.scale = object->GetScale();
        record.color = object->GetColor();
        record.alphaReference = object->GetAlphaReference();
        record.lightingMode = object->GetLightingMode();
        record.textureFilePath = object->GetTextureFilePath();
        records.push_back(record);
    }

    if (!SceneSerializer::SaveObjects(inspector.saveFilePath, records)) {
        inspectorStatus_ = "Save failed: cannot write file";
        return false;
    }

    inspectorStatus_ = std::string("Saved: ") + inspector.saveFilePath;
    return true;
}

bool ImGuiManager::LoadInspectorTransforms(
    const ObjectInspectorSettings& inspector)
{
    if (!inspector.saveFilePath) {
        inspectorStatus_ = "Load failed: no path";
        return false;
    }

    std::vector<SceneSerializer::ObjectRecord> records;
    if (!SceneSerializer::LoadObjects(inspector.saveFilePath, records)) {
        inspectorStatus_ = "Load failed: file not found";
        return false;
    }

    int loadedCount = 0;
    for (int index = 0; index < inspector.objectCount; ++index) {
        Object3d* object = inspector.objects[index].object;
        if (!object) {
            continue;
        }

        const SceneSerializer::ObjectRecord* record =
            SceneSerializer::FindObjectByName(
                records,
                inspector.objects[index].name);
        if (!record) {
            continue;
        }

        object->SetTranslate(record->translate);
        object->SetRotate(record->rotate);
        object->SetScale(record->scale);
        object->SetColor(record->color);
        object->SetAlphaReference(record->alphaReference);
        object->SetLightingMode(record->lightingMode);
        if (!record->textureFilePath.empty()) {
            object->SetTextureFilePath(record->textureFilePath);
        }
        ++loadedCount;
    }

    inspectorStatus_ =
        "Loaded objects: " + std::to_string(loadedCount);
    return loadedCount > 0;
}

void ImGuiManager::Initialize(
    [[maybe_unused]] WinApp* winApp,
    [[maybe_unused]] DirectXCommon* dxCommon,
    [[maybe_unused]] SrvManager* srvManager)
{
#ifdef USE_IMGUI
    assert(winApp);
    assert(dxCommon);
    assert(srvManager);

    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(winApp->GetHwnd());

    srvHeap_ = srvManager->GetDescriptorHeapComPtr();

    ImGui_ImplDX12_InitInfo initInfo{};
    initInfo.Device = dxCommon_->GetDevice();
    initInfo.CommandQueue = dxCommon_->GetCommandQueue();
    initInfo.NumFramesInFlight =
        static_cast<int>(dxCommon_->GetSwapChainResourcesNum());
    initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    initInfo.SrvDescriptorHeap = srvHeap_.Get();
    initInfo.SrvDescriptorAllocFn = AllocateImGuiSrvDescriptor;
    initInfo.SrvDescriptorFreeFn = FreeImGuiSrvDescriptor;
    initInfo.UserData = srvManager_;

    ImGui_ImplDX12_Init(&initInfo);
#endif
}

void ImGuiManager::Begin() {
#ifdef USE_IMGUI
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
#endif
}

void ImGuiManager::End() {
#ifdef USE_IMGUI
    ImGui::Render();
#endif
}

void ImGuiManager::Draw() {
#ifdef USE_IMGUI
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    assert(commandList);

    ID3D12DescriptorHeap* ppHeaps[] = { srvHeap_.Get() };
    commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
#endif
}

void ImGuiManager::Finalize() {
#ifdef USE_IMGUI
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
#endif
}

void ImGuiManager::ShowSpriteController(Math::Vector2& spritePos) {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(500, 100), ImGuiCond_Always);
    ImGui::Begin("Sprite Controller", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::SliderFloat("X", &spritePos.x, 0.0f, 1280.0f);
    ImGui::SliderFloat("Y", &spritePos.y, 0.0f, 720.0f);
    ImGui::Text("Pos : %07.1f , %07.1f", spritePos.x, spritePos.y);

    ImGui::End();
#else
    (void)spritePos;
#endif
}

void ImGuiManager::ShowGamePlayController(GamePlayDebugSettings& settings)
{
    auto& rendering = settings.rendering;
    auto& objects = settings.objects;
    auto& cylinder = settings.cylinder;
    auto& lighting = settings.lighting;
    auto& panels = settings.panels;
    auto& inspector = settings.inspector;
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(260, 360), ImGuiCond_Once);
    ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoCollapse);
    for (int index = 0; index < inspector.objectCount; ++index) {
        const bool isSelected = inspector.selectedObjectIndex == index;
        ImGui::PushID(index);
        if (ImGui::Selectable(inspector.objects[index].name, isSelected)) {
            inspector.selectedObjectIndex = index;
        }
        ImGui::PopID();
    }
    ImGui::End();

    ImGui::SetNextWindowSize(ImVec2(520, 420), ImGuiCond_Once);
    ImGui::Begin("GamePlay Controller", nullptr,
        ImGuiWindowFlags_NoCollapse);

    if (inspector.isPlayMode) {
        if (ImGui::Button("Stop")) {
            inspector.isPlayMode = false;
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("Play Mode");
    } else {
        if (ImGui::Button("Play")) {
            inspector.isPlayMode = true;
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("Edit Mode");
    }
    ImGui::Separator();

    ImGui::SetNextItemOpen(panels.renderingOpen, ImGuiCond_Always);
    panels.renderingOpen = ImGui::CollapsingHeader("Rendering");
    if (panels.renderingOpen) {
    // 3DモデルのBlendModeを切り替え
    const char* blendItems[] = {
        "None", "Normal", "Add", "Subtract", "Multiply", "Screen"
    };
    ImGui::Combo(
        "BlendMode",
        &rendering.blendModeIndex,
        blendItems,
        IM_ARRAYSIZE(blendItems)
    );
    ImGui::SliderFloat(
        "Environment Coefficient",
        &rendering.environmentCoefficient,
        0.0f,
        1.0f
    );
    ImGui::Checkbox("Show Skybox", &rendering.showSkybox);
    const char* postEffectItems[] = {
        "None",
        "Grayscale",
        "Vignette",
        "Box Filter 3x3",
        "Box Filter 5x5",
        "Gaussian Filter",
        "Luminance Outline",
        "Depth Outline",
        "Radial Blur",
        "Dissolve",
        "Random"
    };
    ImGui::Combo(
        "Post Effect",
        &rendering.postEffectMode,
        postEffectItems,
        IM_ARRAYSIZE(postEffectItems)
    );
    }

    ImGui::SetNextItemOpen(panels.objectsOpen, ImGuiCond_Always);
    panels.objectsOpen = ImGui::CollapsingHeader("Objects");
    if (panels.objectsOpen) {
    ImGui::BeginDisabled(inspector.isPlayMode);
    ImGui::Checkbox("Show Plane", &objects.showPlane);

    ImGui::Separator();

    // Planeの回転を調整
    ImGui::Text("Object Rotate");
    ImGui::SliderFloat("Rotate X", &objects.objectRotate.x, -3.14f, 3.14f);
    ImGui::SliderFloat("Rotate Y", &objects.objectRotate.y, -3.14f, 3.14f);
    ImGui::SliderFloat("Rotate Z", &objects.objectRotate.z, -3.14f, 3.14f);
    ImGui::EndDisabled();
    }

    ImGui::Separator();

    ImGui::SetNextItemOpen(panels.inspectorOpen, ImGuiCond_Always);
    panels.inspectorOpen = ImGui::CollapsingHeader("Object Inspector");
    if (panels.inspectorOpen) {
        if (0 <= inspector.selectedObjectIndex &&
            inspector.selectedObjectIndex < inspector.objectCount) {
            Object3d* selectedObject =
                inspector.objects[inspector.selectedObjectIndex].object;
            if (selectedObject) {
                ImGui::TextUnformatted(
                    inspector.objects[inspector.selectedObjectIndex].name);
                ImGui::BeginDisabled(inspector.isPlayMode);
                std::string* editableName =
                    inspector.objects[inspector.selectedObjectIndex]
                    .editableName;
                if (editableName) {
                    char nameBuffer[128]{};
                    std::snprintf(
                        nameBuffer,
                        sizeof(nameBuffer),
                        "%s",
                        editableName->c_str());
                    if (ImGui::InputText(
                        "Name",
                        nameBuffer,
                        sizeof(nameBuffer))) {
                        *editableName = nameBuffer;
                    }
                }

                const char* modelItems[] = {
                    "primitive_plane",
                    "primitive_ring",
                    "primitive_cylinder",
                    "primitive_sphere",
                    "primitive_triangle",
                    "primitive_circle",
                    "primitive_box",
                    "primitive_torus",
                    "primitive_cone",
                    "AnimatedCube/AnimatedCube.gltf",
                    "simpleSkin/simpleSkin.gltf",
                    "human/sneakWalk.gltf",
                    "human/walk.gltf"
                };
                int& selectedModelIndex =
                    *inspector.objects[inspector.selectedObjectIndex]
                    .selectedModelIndex;
                if (ImGui::Combo(
                    "Model",
                    &selectedModelIndex,
                    modelItems,
                    IM_ARRAYSIZE(modelItems))) {
                    selectedObject->SetModel(modelItems[selectedModelIndex]);
                }

                Math::Vector3 scale = selectedObject->GetScale();
                Math::Vector3 rotate = selectedObject->GetRotate();
                Math::Vector3 translate = selectedObject->GetTranslate();

                const char* gizmoModeItems[] = {
                    "Translate",
                    "Rotate",
                    "Scale"
                };
                ImGui::Combo(
                    "Gizmo Mode",
                    &inspector.gizmoMode,
                    gizmoModeItems,
                    IM_ARRAYSIZE(gizmoModeItems));

                if (ImGui::DragFloat3("Translate", &translate.x, 0.05f)) {
                    selectedObject->SetTranslate(translate);
                }
                if (ImGui::DragFloat3("Rotate", &rotate.x, 0.02f)) {
                    selectedObject->SetRotate(rotate);
                }
                if (ImGui::DragFloat3("Scale", &scale.x, 0.02f, 0.01f, 20.0f)) {
                    selectedObject->SetScale(scale);
                }

                const float translateStep = 0.1f;
                const float rotateStep = 0.05f;
                const float scaleStep = 0.1f;
                Math::Vector3* activeTransform = &translate;
                float step = translateStep;
                if (inspector.gizmoMode == 1) {
                    activeTransform = &rotate;
                    step = rotateStep;
                } else if (inspector.gizmoMode == 2) {
                    activeTransform = &scale;
                    step = scaleStep;
                }

                auto applyGizmoTransform = [&]() {
                    if (inspector.gizmoMode == 0) {
                        selectedObject->SetTranslate(translate);
                    } else if (inspector.gizmoMode == 1) {
                        selectedObject->SetRotate(rotate);
                    } else {
                        scale.x = scale.x < 0.01f ? 0.01f : scale.x;
                        scale.y = scale.y < 0.01f ? 0.01f : scale.y;
                        scale.z = scale.z < 0.01f ? 0.01f : scale.z;
                        selectedObject->SetScale(scale);
                    }
                };

                ImGui::TextUnformatted("Gizmo Nudge");
                if (ImGui::Button("-X")) {
                    activeTransform->x -= step;
                    applyGizmoTransform();
                }
                ImGui::SameLine();
                if (ImGui::Button("+X")) {
                    activeTransform->x += step;
                    applyGizmoTransform();
                }
                ImGui::SameLine();
                if (ImGui::Button("-Y")) {
                    activeTransform->y -= step;
                    applyGizmoTransform();
                }
                ImGui::SameLine();
                if (ImGui::Button("+Y")) {
                    activeTransform->y += step;
                    applyGizmoTransform();
                }
                ImGui::SameLine();
                if (ImGui::Button("-Z")) {
                    activeTransform->z -= step;
                    applyGizmoTransform();
                }
                ImGui::SameLine();
                if (ImGui::Button("+Z")) {
                    activeTransform->z += step;
                    applyGizmoTransform();
                }

                ImGui::SetNextItemOpen(panels.materialOpen, ImGuiCond_Always);
                panels.materialOpen = ImGui::CollapsingHeader("Material");
                if (panels.materialOpen) {
                    Math::Vector4 color = selectedObject->GetColor();
                    if (ImGui::ColorEdit4("Color", &color.x)) {
                        selectedObject->SetColor(color);
                    }

                    float alphaReference = selectedObject->GetAlphaReference();
                    if (ImGui::SliderFloat(
                        "Alpha Ref",
                        &alphaReference,
                        0.0f,
                        1.0f)) {
                        selectedObject->SetAlphaReference(alphaReference);
                    }

                    const char* lightingItems[] = {
                        "None",
                        "Lambert",
                        "Half Lambert"
                    };
                    int lightingMode = selectedObject->GetLightingMode();
                    if (ImGui::Combo(
                        "Lighting##MaterialLighting",
                        &lightingMode,
                        lightingItems,
                        IM_ARRAYSIZE(lightingItems))) {
                        selectedObject->SetLightingMode(lightingMode);
                    }

                    const char* textureItems[] = {
                        "resources/uvChecker.png",
                        "resources/checkerBoard.png",
                        "resources/gradationLine.png",
                        "resources/circle2.png",
                        "resources/monsterBall.png",
                        "resources/human/white.png"
                    };
                    int textureIndex = 0;
                    const std::string& texturePath =
                        selectedObject->GetTextureFilePath();
                    for (int index = 0; index < IM_ARRAYSIZE(textureItems); ++index) {
                        if (texturePath == textureItems[index]) {
                            textureIndex = index;
                            break;
                        }
                    }
                    if (ImGui::Combo(
                        "Texture",
                        &textureIndex,
                        textureItems,
                        IM_ARRAYSIZE(textureItems))) {
                        selectedObject->SetTextureFilePath(textureItems[textureIndex]);
                    }
                }
                ImGui::EndDisabled();
            }
        }

        const char* modelItems[] = {
            "primitive_plane",
            "primitive_ring",
            "primitive_cylinder",
            "primitive_sphere",
            "primitive_triangle",
            "primitive_circle",
            "primitive_box",
            "primitive_torus",
            "primitive_cone",
            "AnimatedCube/AnimatedCube.gltf",
            "simpleSkin/simpleSkin.gltf",
            "human/sneakWalk.gltf",
            "human/walk.gltf"
        };
        ImGui::Separator();
        if (inspector.sceneFileItems && inspector.sceneFileItemCount > 0) {
            ImGui::Combo(
                "Scene Slot",
                &inspector.sceneFileIndex,
                inspector.sceneFileItems,
                inspector.sceneFileItemCount);
        }
        if (inspector.prefabFileItems && inspector.prefabFileItemCount > 0) {
            ImGui::Combo(
                "Prefab Slot",
                &inspector.prefabFileIndex,
                inspector.prefabFileItems,
                inspector.prefabFileItemCount);
        }
        ImGui::BeginDisabled(inspector.isPlayMode);
        ImGui::Combo(
            "Add Model",
            &inspector.addModelIndex,
            modelItems,
            IM_ARRAYSIZE(modelItems)
        );
        if (ImGui::Button("Add Primitive")) {
            inspector.requestAddObject = true;
        }

        if (ImGui::Button("Undo Transform")) {
            inspector.requestUndo = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Redo Transform")) {
            inspector.requestRedo = true;
        }

        if (ImGui::Button("Save Transforms")) {
            inspector.requestSaveObjects = true;
            inspectorStatus_ = std::string("Save requested: ") +
                inspector.saveFilePath;
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Transforms")) {
            inspector.requestLoadObjects = true;
            inspectorStatus_ = std::string("Load requested: ") +
                inspector.saveFilePath;
        }
        if (inspector.selectedObjectIndex >= 1) {
            ImGui::SameLine();
            if (ImGui::Button("Duplicate Selected")) {
                inspector.requestDuplicateObject = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove Selected")) {
                inspector.requestRemoveObject = true;
            }
            if (ImGui::Button("Save Prefab")) {
                inspector.requestSavePrefab = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Instantiate Prefab")) {
                inspector.requestInstantiatePrefab = true;
            }
        }
        ImGui::EndDisabled();
        if (!inspectorStatus_.empty()) {
            ImGui::TextUnformatted(inspectorStatus_.c_str());
        }
    }

    ImGui::Separator();

    ImGui::SetNextItemOpen(panels.cylinderOpen, ImGuiCond_Always);
    panels.cylinderOpen = ImGui::CollapsingHeader("Cylinder Effect");
    if (panels.cylinderOpen) {
    ImGui::BeginDisabled(inspector.isPlayMode);
    ImGui::Text("Cylinder Effect");
    ImGui::ColorEdit4("Cylinder Color", &cylinder.cylinderColor.x);
    ImGui::SliderFloat(
        "Cylinder Alpha Ref",
        &cylinder.cylinderAlphaReference,
        0.0f,
        1.0f
    );
    ImGui::SliderFloat(
        "Cylinder UV Speed",
        &cylinder.cylinderUVScrollSpeed,
        -2.0f,
        2.0f
    );
    ImGui::EndDisabled();

    }

    ImGui::Separator();

    ImGui::SetNextItemOpen(panels.lightingOpen, ImGuiCond_Always);
    panels.lightingOpen = ImGui::CollapsingHeader("Lighting");
    if (panels.lightingOpen) {
    ImGui::BeginDisabled(inspector.isPlayMode);
    // DirectionalLightの向きと強さを調整
    ImGui::Text("Directional Light");
    ImGui::SliderFloat("Light X", &lighting.lightDirection.x, -1.0f, 1.0f);
    ImGui::SliderFloat("Light Y", &lighting.lightDirection.y, -1.0f, 1.0f);
    ImGui::SliderFloat("Light Z", &lighting.lightDirection.z, -1.0f, 1.0f);
    ImGui::SliderFloat("Intensity", &lighting.lightIntensity, 0.0f, 5.0f);

    ImGui::Separator();

    // PointLightの位置と強さを調整
    ImGui::Text("Point Light");
    ImGui::SliderFloat("Point X", &lighting.pointLightPosition.x, -10.0f, 10.0f);
    ImGui::SliderFloat("Point Y", &lighting.pointLightPosition.y, -10.0f, 10.0f);
    ImGui::SliderFloat("Point Z", &lighting.pointLightPosition.z, -10.0f, 10.0f);
    ImGui::SliderFloat("Point Intensity", &lighting.pointLightIntensity, 0.0f, 10.0f);

    ImGui::Separator();

    // SpotLightの位置、向き、強さを調整
    ImGui::Text("Spot Light");
    ImGui::SliderFloat("Spot X", &lighting.spotLightPosition.x, -10.0f, 10.0f);
    ImGui::SliderFloat("Spot Y", &lighting.spotLightPosition.y, -10.0f, 10.0f);
    ImGui::SliderFloat("Spot Z", &lighting.spotLightPosition.z, -10.0f, 10.0f);
    ImGui::SliderFloat("Spot Dir X", &lighting.spotLightDirection.x, -1.0f, 1.0f);
    ImGui::SliderFloat("Spot Dir Y", &lighting.spotLightDirection.y, -1.0f, 1.0f);
    ImGui::SliderFloat("Spot Dir Z", &lighting.spotLightDirection.z, -1.0f, 1.0f);
    ImGui::SliderFloat("Spot Intensity", &lighting.spotLightIntensity, 0.0f, 20.0f);
    ImGui::EndDisabled();
    }

    ImGui::End();
#else
    (void)rendering;
    (void)objects;
    (void)cylinder;
    (void)lighting;
    (void)panels;
    (void)inspector;
#endif
}
