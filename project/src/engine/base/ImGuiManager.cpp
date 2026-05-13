#include "engine/base/ImGuiManager.h"
#include "engine/base/WinApp.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/SrvManager.h"
#include "engine/3d/Object3d.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>

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

bool ExtractJsonVector3(
    const std::string& source,
    const char* key,
    Math::Vector3& out)
{
    const std::regex pattern(
        std::string("\"") + key +
        "\"\\s*:\\s*\\[\\s*([-+0-9.eE]+)\\s*,\\s*([-+0-9.eE]+)\\s*,\\s*([-+0-9.eE]+)\\s*\\]"
    );
    std::smatch match;
    if (!std::regex_search(source, match, pattern)) {
        return false;
    }

    out.x = std::stof(match[1].str());
    out.y = std::stof(match[2].str());
    out.z = std::stof(match[3].str());
    return true;
}

}

bool ImGuiManager::SaveInspectorTransforms(
    const ObjectInspectorSettings& inspector)
{
    if (!inspector.saveFilePath) {
        inspectorStatus_ = "Save failed: no path";
        return false;
    }

    std::filesystem::path path(inspector.saveFilePath);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream file(path);
    if (!file) {
        inspectorStatus_ = "Save failed: cannot open file";
        return false;
    }

    file << std::fixed << std::setprecision(6);
    file << "{\n";
    file << "  \"objects\": [\n";
    for (int index = 0; index < inspector.objectCount; ++index) {
        Object3d* object = inspector.objects[index].object;
        if (!object) {
            continue;
        }

        const Math::Vector3 translate = object->GetTranslate();
        const Math::Vector3 rotate = object->GetRotate();
        const Math::Vector3 scale = object->GetScale();
        const bool isPrimitive = index >= 8;
        const int modelIndex =
            inspector.objects[index].selectedModelIndex ?
            *inspector.objects[index].selectedModelIndex :
            -1;

        file << "    {\n";
        file << "      \"name\": \"" << inspector.objects[index].name << "\",\n";
        file << "      \"primitive\": " << (isPrimitive ? "true" : "false") << ",\n";
        file << "      \"modelIndex\": " << modelIndex << ",\n";
        file << "      \"translate\": [" << translate.x << ", "
            << translate.y << ", " << translate.z << "],\n";
        file << "      \"rotate\": [" << rotate.x << ", "
            << rotate.y << ", " << rotate.z << "],\n";
        file << "      \"scale\": [" << scale.x << ", "
            << scale.y << ", " << scale.z << "]\n";
        file << "    }" << (index + 1 < inspector.objectCount ? "," : "") << "\n";
    }
    file << "  ]\n";
    file << "}\n";

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

    std::ifstream file(inspector.saveFilePath);
    if (!file) {
        inspectorStatus_ = "Load failed: file not found";
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string json = buffer.str();

    int loadedCount = 0;
    for (int index = 0; index < inspector.objectCount; ++index) {
        Object3d* object = inspector.objects[index].object;
        if (!object) {
            continue;
        }

        const std::regex objectPattern(
            std::string("\\{[^{}]*\"name\"\\s*:\\s*\"") +
            inspector.objects[index].name + "\"[^{}]*\\}"
        );
        std::smatch objectMatch;
        if (!std::regex_search(json, objectMatch, objectPattern)) {
            continue;
        }

        const std::string objectJson = objectMatch[0].str();
        Math::Vector3 translate{};
        Math::Vector3 rotate{};
        Math::Vector3 scale{};
        if (!ExtractJsonVector3(objectJson, "translate", translate) ||
            !ExtractJsonVector3(objectJson, "rotate", rotate) ||
            !ExtractJsonVector3(objectJson, "scale", scale)) {
            continue;
        }

        object->SetTranslate(translate);
        object->SetRotate(rotate);
        object->SetScale(scale);
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

    if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
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

    if (ImGui::CollapsingHeader("Objects", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Checkbox("Show Plane", &objects.showPlane);
    ImGui::Checkbox("Show Ring", &objects.showRing);
    ImGui::Checkbox("Show Cylinder", &objects.showCylinder);
    ImGui::Checkbox("Show Sphere", &objects.showSphere);
    ImGui::Checkbox("Show Particle", &objects.showParticle);

    ImGui::Separator();

    // Planeの回転を調整
    ImGui::Text("Object Rotate");
    ImGui::SliderFloat("Rotate X", &objects.objectRotate.x, -3.14f, 3.14f);
    ImGui::SliderFloat("Rotate Y", &objects.objectRotate.y, -3.14f, 3.14f);
    ImGui::SliderFloat("Rotate Z", &objects.objectRotate.z, -3.14f, 3.14f);
    }

    ImGui::Separator();

    if (ImGui::CollapsingHeader("Object Inspector", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (0 <= inspector.selectedObjectIndex &&
            inspector.selectedObjectIndex < inspector.objectCount) {
            Object3d* selectedObject =
                inspector.objects[inspector.selectedObjectIndex].object;
            if (selectedObject) {
                ImGui::TextUnformatted(
                    inspector.objects[inspector.selectedObjectIndex].name);
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

                if (ImGui::DragFloat3("Translate", &translate.x, 0.05f)) {
                    selectedObject->SetTranslate(translate);
                }
                if (ImGui::DragFloat3("Rotate", &rotate.x, 0.02f)) {
                    selectedObject->SetRotate(rotate);
                }
                if (ImGui::DragFloat3("Scale", &scale.x, 0.02f, 0.01f, 20.0f)) {
                    selectedObject->SetScale(scale);
                }
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
        ImGui::Combo(
            "Add Model",
            &inspector.addModelIndex,
            modelItems,
            IM_ARRAYSIZE(modelItems)
        );
        if (ImGui::Button("Add Primitive")) {
            inspector.requestAddObject = true;
        }

        if (ImGui::Button("Save Transforms")) {
            SaveInspectorTransforms(inspector);
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Transforms")) {
            if (LoadInspectorTransforms(inspector)) {
                inspector.requestLoadObjects = true;
            }
        }
        if (inspector.selectedObjectIndex >= 8) {
            ImGui::SameLine();
            if (ImGui::Button("Remove Selected")) {
                inspector.requestRemoveObject = true;
            }
        }
        if (!inspectorStatus_.empty()) {
            ImGui::TextUnformatted(inspectorStatus_.c_str());
        }
    }

    ImGui::Separator();

    if (ImGui::CollapsingHeader("Cylinder Effect")) {
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

    }

    ImGui::Separator();

    if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    }

    ImGui::End();
#else
    (void)rendering;
    (void)objects;
    (void)cylinder;
    (void)lighting;
    (void)inspector;
#endif
}
