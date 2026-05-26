#include "engine/base/ImGuiManager.h"
#include "engine/base/WinApp.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/SrvManager.h"
#include "engine/3d/Object3d.h"
#include "engine/scene/SceneSerializer.h"

#include <cassert>
#include <cstdio>
#include <cstring>

namespace {

#ifdef USE_IMGUI

constexpr float kEditorToolbarHeight = 54.0f;
constexpr float kEditorHierarchyWidth = 240.0f;
constexpr float kEditorInspectorWidth = 330.0f;
constexpr float kEditorClientWidth = 1280.0f;
constexpr float kEditorClientHeight = 720.0f;
constexpr float kEditorPanelTop = kEditorToolbarHeight;
constexpr float kEditorPanelHeight = kEditorClientHeight - kEditorToolbarHeight;
constexpr float kEditorProjectHeight = 170.0f;
constexpr float kEditorCenterX = kEditorHierarchyWidth;
constexpr float kEditorCenterWidth =
    kEditorClientWidth - kEditorHierarchyWidth - kEditorInspectorWidth;
constexpr float kEditorViewportHeight =
    kEditorPanelHeight - kEditorProjectHeight;
constexpr float kEditorProjectY =
    kEditorPanelTop + kEditorViewportHeight;
constexpr float kEditorInspectorX = kEditorClientWidth - kEditorInspectorWidth;

constexpr const char* kProjectModelAssets[] = {
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

constexpr const char* kProjectTextureAssets[] = {
    "resources/uvChecker.png",
    "resources/checkerBoard.png",
    "resources/gradationLine.png",
    "resources/circle2.png",
    "resources/monsterBall.png",
    "resources/human/white.png"
};

bool DrawToolbarToolButton(
    const char* label,
    const char* tooltip,
    int toolIndex,
    int& activeToolIndex)
{
    const bool isActive = activeToolIndex == toolIndex;
    if (isActive) {
        ImGui::PushStyleColor(
            ImGuiCol_Button,
            ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    }

    const bool clicked = ImGui::Button(label, ImVec2(28.0f, 0.0f));
    if (clicked) {
        activeToolIndex = toolIndex;
    }

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }

    if (isActive) {
        ImGui::PopStyleColor();
    }

    return clicked;
}

bool BeginInspectorComponent(const char* label)
{
    ImGui::Separator();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 6.0f));
    const bool isOpen = ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen);
    ImGui::PopStyleVar();
    return isOpen;
}

bool MatchesProjectFilter(const char* text, const char* filter)
{
    return !filter || filter[0] == '\0' || std::strstr(text, filter);
}

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

    // ImGui縺御ｸ崎ｦ√↓縺ｪ縺｣縺欖RV繧偵∵ｬ｡縺ｮ遒ｺ菫昴〒蜀榊茜逕ｨ縺ｧ縺阪ｋ繧医≧縺ｫ謌ｻ縺・
    srvManager->Free(cpuHandle);
}

#endif
}

bool ImGuiManager::SaveInspectorTransforms(
    const ObjectInspectorSettings& inspector)
{
    if (!inspector.saveFilePath) {
        inspectorStatus_ = "保存失敗: パスがありません";
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
        inspectorStatus_ = "保存失敗: ファイルを書き込めません";
        return false;
    }

    inspectorStatus_ = std::string("保存しました: ") + inspector.saveFilePath;
    return true;
}

bool ImGuiManager::LoadInspectorTransforms(
    const ObjectInspectorSettings& inspector)
{
    if (!inspector.saveFilePath) {
        inspectorStatus_ = "読み込み失敗: パスがありません";
        return false;
    }

    std::vector<SceneSerializer::ObjectRecord> records;
    if (!SceneSerializer::LoadObjects(inspector.saveFilePath, records)) {
        inspectorStatus_ = "読み込み失敗: ファイルが見つかりません";
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
        "読み込みオブジェクト数: " + std::to_string(loadedCount);
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
    ImGuiIO& io = ImGui::GetIO();
    ImFont* japaneseFont = io.Fonts->AddFontFromFileTTF(
        "C:/Windows/Fonts/meiryo.ttc",
        16.0f,
        nullptr,
        io.Fonts->GetGlyphRangesJapanese());
    if (japaneseFont) {
        io.FontDefault = japaneseFont;
    } else {
        io.Fonts->AddFontDefault();
    }

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
    ImGui::Begin("スプライト操作", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::SliderFloat("X", &spritePos.x, 0.0f, 1280.0f);
    ImGui::SliderFloat("Y", &spritePos.y, 0.0f, 720.0f);
    ImGui::Text("座標 : %07.1f , %07.1f", spritePos.x, spritePos.y);

    ImGui::End();
#else
    (void)spritePos;
#endif
}

void ImGuiManager::ShowEditorController(EditorDebugSettings& settings)
{
    auto& rendering = settings.rendering;
    auto& cylinder = settings.cylinder;
    auto& lighting = settings.lighting;
    auto& panels = settings.panels;
    auto& inspector = settings.inspector;
#ifdef USE_IMGUI
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        ImVec2(kEditorClientWidth, kEditorToolbarHeight),
        ImGuiCond_Always);
    ImGui::Begin(
        "Toolbar",
        nullptr,
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_MenuBar);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("ファイル")) {
            ImGui::BeginDisabled(inspector.isPlayMode);
            if (ImGui::MenuItem("シーンを保存")) {
                inspector.requestSaveObjects = true;
            }
            if (ImGui::MenuItem("シーンを読み込み")) {
                inspector.requestLoadObjects = true;
            }
            if (ImGui::MenuItem("プレハブを保存")) {
                inspector.requestSavePrefab = true;
            }
            ImGui::EndDisabled();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("編集")) {
            ImGui::BeginDisabled(inspector.isPlayMode);
            if (ImGui::MenuItem("元に戻す")) {
                inspector.requestUndo = true;
            }
            if (ImGui::MenuItem("やり直す")) {
                inspector.requestRedo = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("プリミティブを追加")) {
                inspector.requestAddObject = true;
            }
            if (ImGui::MenuItem("選択中を複製", nullptr, false,
                inspector.selectedObjectIndex >= 0)) {
                inspector.requestDuplicateObject = true;
            }
            if (ImGui::MenuItem("選択中を削除", nullptr, false,
                inspector.selectedObjectIndex >= 0)) {
                inspector.requestRemoveObject = true;
            }
            ImGui::EndDisabled();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("ゲームオブジェクト")) {
            ImGui::BeginDisabled(inspector.isPlayMode);
            for (int index = 0; index < IM_ARRAYSIZE(kProjectModelAssets); ++index) {
                if (ImGui::MenuItem(kProjectModelAssets[index])) {
                    inspector.addModelIndex = index;
                    inspector.requestAddObject = true;
                    inspectorStatus_ = std::string("モデル追加: ") +
                        kProjectModelAssets[index];
                }
            }
            ImGui::EndDisabled();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("ウィンドウ")) {
            ImGui::MenuItem("ゲームビュー", nullptr, &panels.gameViewOpen);
            ImGui::MenuItem("描画", nullptr, &panels.renderingOpen);
            ImGui::MenuItem("オブジェクト", nullptr, &panels.objectsOpen);
            ImGui::MenuItem(
                "オブジェクトインスペクター",
                nullptr,
                &panels.inspectorOpen);
            ImGui::MenuItem("マテリアル", nullptr, &panels.materialOpen);
            ImGui::MenuItem(
                "シリンダーエフェクト",
                nullptr,
                &panels.cylinderOpen);
            ImGui::MenuItem("ライティング", nullptr, &panels.lightingOpen);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("再生")) {
            if (inspector.isPlayMode) {
                if (ImGui::MenuItem("停止")) {
                    inspector.requestStopPlayMode = true;
                }
            } else {
                if (ImGui::MenuItem("再生")) {
                    inspector.requestStartPlayMode = true;
                }
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    ImGui::BeginDisabled(inspector.isPlayMode);
    DrawToolbarToolButton("移", "移動ツール (1)", 0, inspector.gizmoMode);
    ImGui::SameLine();
    DrawToolbarToolButton("回", "回転ツール (2)", 1, inspector.gizmoMode);
    ImGui::SameLine();
    DrawToolbarToolButton("拡", "拡縮ツール (3)", 2, inspector.gizmoMode);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextUnformatted("|");
    ImGui::SameLine();

    if (inspector.isPlayMode) {
        if (ImGui::Button("停止")) {
            inspector.requestStopPlayMode = true;
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("再生モード");
    } else {
        if (ImGui::Button("再生")) {
            inspector.requestStartPlayMode = true;
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("編集モード");
    }
    ImGui::SameLine();
    ImGui::Checkbox("ゲームビュー", &panels.gameViewOpen);
    ImGui::SameLine();
    if (ImGui::RadioButton("シーン", panels.viewportTabIndex == 0)) {
        panels.viewportTabIndex = 0;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("ゲーム", panels.viewportTabIndex == 1)) {
        panels.viewportTabIndex = 1;
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(
        inspector.isPlayMode ?
        "F1 表示 / F2 停止 / F4 ゲームデバッグ" :
        "F1 表示 / F2 再生 / F3 シーンカメラ");
    ImGui::End();

    ImGui::SetNextWindowPos(
        ImVec2(0.0f, kEditorPanelTop),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        ImVec2(kEditorHierarchyWidth, kEditorPanelHeight),
        ImGuiCond_Always);
    ImGui::Begin(
        "ヒエラルキー",
        nullptr,
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove);

    static char hierarchyFilter[64] = {};
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint(
        "##HierarchySearch",
        "検索",
        hierarchyFilter,
        sizeof(hierarchyFilter));
    ImGui::BeginDisabled(inspector.isPlayMode);
    if (ImGui::Button("+", ImVec2(28.0f, 0.0f))) {
        inspector.requestAddObject = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("プリミティブを追加");
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(inspector.selectedObjectIndex < 1);
    if (ImGui::Button("D", ImVec2(28.0f, 0.0f))) {
        inspector.requestDuplicateObject = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("選択中を複製");
    }
    ImGui::SameLine();
    if (ImGui::Button("-", ImVec2(28.0f, 0.0f))) {
        inspector.requestRemoveObject = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("選択中を削除");
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    ImGui::Separator();

    for (int index = 0; index < inspector.objectCount; ++index) {
        const char* objectName = inspector.objects[index].name;
        if (hierarchyFilter[0] != '\0' &&
            std::strstr(objectName, hierarchyFilter) == nullptr) {
            continue;
        }

        const bool isSelected = inspector.selectedObjectIndex == index;
        ImGui::PushID(index);
        bool* visible = inspector.objects[index].visible;
        ImGui::BeginDisabled(inspector.isPlayMode || !visible);
        if (ImGui::SmallButton(visible && *visible ? "◎" : "○")) {
            *visible = !*visible;
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                visible && *visible ? "表示中" : "非表示");
        }
        ImGui::SameLine();
        if (ImGui::Selectable(objectName, isSelected)) {
            inspector.selectedObjectIndex = index;
        }
        if (ImGui::BeginPopupContextItem("HierarchyObjectContext")) {
            inspector.selectedObjectIndex = index;
            ImGui::BeginDisabled(inspector.isPlayMode || !visible);
            if (visible && ImGui::MenuItem(*visible ? "非表示" : "表示")) {
                *visible = !*visible;
            }
            ImGui::EndDisabled();
            ImGui::BeginDisabled(inspector.isPlayMode || index < 1);
            if (ImGui::MenuItem("複製")) {
                inspector.requestDuplicateObject = true;
            }
            if (ImGui::MenuItem("削除")) {
                inspector.requestRemoveObject = true;
            }
            ImGui::EndDisabled();
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }

    if (ImGui::BeginPopupContextWindow(
        "HierarchyContext",
        ImGuiPopupFlags_MouseButtonRight |
        ImGuiPopupFlags_NoOpenOverItems)) {
        ImGui::BeginDisabled(inspector.isPlayMode);
        if (ImGui::BeginMenu("作成")) {
            for (int index = 0; index < IM_ARRAYSIZE(kProjectModelAssets); ++index) {
                if (ImGui::MenuItem(kProjectModelAssets[index])) {
                    inspector.addModelIndex = index;
                    inspector.requestAddObject = true;
                    inspectorStatus_ = std::string("モデル追加: ") +
                        kProjectModelAssets[index];
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }
    ImGui::End();

    ImGui::SetNextWindowPos(
        ImVec2(kEditorInspectorX, kEditorPanelTop),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        ImVec2(kEditorInspectorWidth, kEditorPanelHeight),
        ImGuiCond_Always);
    ImGui::Begin(
        "インスペクター",
        nullptr,
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove);

    ImGui::SetNextItemOpen(panels.renderingOpen, ImGuiCond_Always);
    panels.renderingOpen = ImGui::CollapsingHeader("描画");
    if (panels.renderingOpen) {
    // 3D繝｢繝・Ν縺ｮBlendMode繧貞・繧頑崛縺・
    const char* blendItems[] = {
        "なし", "通常", "加算", "減算", "乗算", "スクリーン"
    };
    ImGui::Combo(
        "ブレンドモード",
        &rendering.blendModeIndex,
        blendItems,
        IM_ARRAYSIZE(blendItems)
    );
    ImGui::SliderFloat(
        "環境光の強さ",
        &rendering.environmentCoefficient,
        0.0f,
        1.0f
    );
    ImGui::Checkbox("スカイボックス表示", &rendering.showSkybox);
    const char* postEffectItems[] = {
        "なし",
        "グレースケール",
        "ビネット",
        "ボックスフィルター 3x3",
        "ボックスフィルター 5x5",
        "ガウシアンフィルター",
        "輝度アウトライン",
        "深度アウトライン",
        "放射ブラー",
        "ディゾルブ",
        "ランダム",
        "ビネット + スムージング"
    };
    ImGui::Combo(
        "ポストエフェクト",
        &rendering.postEffectMode,
        postEffectItems,
        IM_ARRAYSIZE(postEffectItems)
    );
    }

    ImGui::SetNextItemOpen(panels.objectsOpen, ImGuiCond_Always);
    panels.objectsOpen = ImGui::CollapsingHeader("オブジェクト");
    if (panels.objectsOpen) {
        ImGui::Text("シーン内オブジェクト: %d", inspector.objectCount);
    }

    ImGui::Separator();

    ImGui::SetNextItemOpen(panels.inspectorOpen, ImGuiCond_Always);
    panels.inspectorOpen = ImGui::CollapsingHeader("オブジェクトインスペクター");
    if (panels.inspectorOpen) {
        if (0 <= inspector.selectedObjectIndex &&
            inspector.selectedObjectIndex < inspector.objectCount) {
            Object3d* selectedObject =
                inspector.objects[inspector.selectedObjectIndex].object;
            if (selectedObject) {
                ImGui::BeginDisabled(inspector.isPlayMode);
                if (BeginInspectorComponent("基本情報")) {
                ImGui::TextUnformatted(
                    inspector.objects[inspector.selectedObjectIndex].name);
                if (inspector.objects[inspector.selectedObjectIndex].visible) {
                    ImGui::Checkbox(
                        "表示",
                        inspector.objects[inspector.selectedObjectIndex].visible);
                }
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
                        "名前",
                        nameBuffer,
                        sizeof(nameBuffer))) {
                        *editableName = nameBuffer;
                    }
                }
                }

                if (BeginInspectorComponent("Mesh Renderer")) {
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
                    "モデル",
                    &selectedModelIndex,
                    modelItems,
                    IM_ARRAYSIZE(modelItems))) {
                    selectedObject->SetModel(modelItems[selectedModelIndex]);
                }
                }

                Math::Vector3 scale = selectedObject->GetScale();
                Math::Vector3 rotate = selectedObject->GetRotate();
                Math::Vector3 translate = selectedObject->GetTranslate();

                if (BeginInspectorComponent("Transform")) {
                const char* gizmoModeItems[] = {
                    "移動",
                    "回転",
                    "拡縮"
                };
                ImGui::Combo(
                    "操作モード",
                    &inspector.gizmoMode,
                    gizmoModeItems,
                    IM_ARRAYSIZE(gizmoModeItems));

                if (ImGui::DragFloat3("位置", &translate.x, 0.05f)) {
                    selectedObject->SetTranslate(translate);
                }
                if (ImGui::DragFloat3("回転", &rotate.x, 0.02f)) {
                    selectedObject->SetRotate(rotate);
                }
                if (ImGui::DragFloat3("拡縮", &scale.x, 0.02f, 0.01f, 20.0f)) {
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

                ImGui::TextUnformatted("操作微調整");
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
                }

                ImGui::SetNextItemOpen(panels.materialOpen, ImGuiCond_Always);
                panels.materialOpen = BeginInspectorComponent("Material");
                if (panels.materialOpen) {
                    Math::Vector4 color = selectedObject->GetColor();
                    if (ImGui::ColorEdit4("色", &color.x)) {
                        selectedObject->SetColor(color);
                    }

                    float alphaReference = selectedObject->GetAlphaReference();
                    if (ImGui::SliderFloat(
                        "アルファ参照",
                        &alphaReference,
                        0.0f,
                        1.0f)) {
                        selectedObject->SetAlphaReference(alphaReference);
                    }

                    const char* lightingItems[] = {
                        "なし",
                        "ランバート",
                        "ハーフランバート"
                    };
                    int lightingMode = selectedObject->GetLightingMode();
                    if (ImGui::Combo(
                        "ライティング##MaterialLighting",
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
                        "テクスチャ",
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
                "シーン枠",
                &inspector.sceneFileIndex,
                inspector.sceneFileItems,
                inspector.sceneFileItemCount);
        }
        if (inspector.prefabFileItems && inspector.prefabFileItemCount > 0) {
            ImGui::Combo(
                "プレハブ枠",
                &inspector.prefabFileIndex,
                inspector.prefabFileItems,
                inspector.prefabFileItemCount);
        }
        ImGui::BeginDisabled(inspector.isPlayMode);
        ImGui::Combo(
            "追加モデル",
            &inspector.addModelIndex,
            modelItems,
            IM_ARRAYSIZE(modelItems)
        );
        if (ImGui::Button("プリミティブ追加")) {
            inspector.requestAddObject = true;
        }

        if (ImGui::Button("変形を元に戻す")) {
            inspector.requestUndo = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("変形をやり直す")) {
            inspector.requestRedo = true;
        }

        if (ImGui::Button("変形を保存")) {
            inspector.requestSaveObjects = true;
            inspectorStatus_ = std::string("保存リクエスト: ") +
                inspector.saveFilePath;
        }
        ImGui::SameLine();
        if (ImGui::Button("変形を読み込み")) {
            inspector.requestLoadObjects = true;
            inspectorStatus_ = std::string("読み込みリクエスト: ") +
                inspector.saveFilePath;
        }
        if (inspector.selectedObjectIndex >= 0) {
            ImGui::SameLine();
            if (ImGui::Button("選択中を複製")) {
                inspector.requestDuplicateObject = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("選択中を削除")) {
                inspector.requestRemoveObject = true;
            }
            if (ImGui::Button("プレハブ保存")) {
                inspector.requestSavePrefab = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("プレハブ生成")) {
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
    panels.cylinderOpen = ImGui::CollapsingHeader("シリンダーエフェクト");
    if (panels.cylinderOpen) {
    ImGui::BeginDisabled(inspector.isPlayMode);
    ImGui::Text("シリンダーエフェクト");
    ImGui::ColorEdit4("シリンダー色", &cylinder.cylinderColor.x);
    ImGui::SliderFloat(
        "シリンダー アルファ参照",
        &cylinder.cylinderAlphaReference,
        0.0f,
        1.0f
    );
    ImGui::SliderFloat(
        "シリンダーUV速度",
        &cylinder.cylinderUVScrollSpeed,
        -2.0f,
        2.0f
    );
    ImGui::EndDisabled();

    }

    ImGui::Separator();

    ImGui::SetNextItemOpen(panels.lightingOpen, ImGuiCond_Always);
    panels.lightingOpen = ImGui::CollapsingHeader("ライティング");
    if (panels.lightingOpen) {
    ImGui::BeginDisabled(inspector.isPlayMode);
    // DirectionalLight縺ｮ蜷代″縺ｨ蠑ｷ縺輔ｒ隱ｿ謨ｴ
    ImGui::Text("平行光源");
    ImGui::SliderFloat("光向き X", &lighting.lightDirection.x, -1.0f, 1.0f);
    ImGui::SliderFloat("光向き Y", &lighting.lightDirection.y, -1.0f, 1.0f);
    ImGui::SliderFloat("光向き Z", &lighting.lightDirection.z, -1.0f, 1.0f);
    ImGui::SliderFloat("強さ##DirectionalIntensity", &lighting.lightIntensity, 0.0f, 5.0f);

    ImGui::Separator();

    // PointLight縺ｮ菴咲ｽｮ縺ｨ蠑ｷ縺輔ｒ隱ｿ謨ｴ
    ImGui::Text("点光源");
    ImGui::SliderFloat("点光源 X", &lighting.pointLightPosition.x, -10.0f, 10.0f);
    ImGui::SliderFloat("点光源 Y", &lighting.pointLightPosition.y, -10.0f, 10.0f);
    ImGui::SliderFloat("点光源 Z", &lighting.pointLightPosition.z, -10.0f, 10.0f);
    ImGui::SliderFloat("点光源の強さ", &lighting.pointLightIntensity, 0.0f, 10.0f);

    ImGui::Separator();

    // SpotLight縺ｮ菴咲ｽｮ縲∝髄縺阪∝ｼｷ縺輔ｒ隱ｿ謨ｴ
    ImGui::Text("スポットライト");
    ImGui::SliderFloat("スポット X", &lighting.spotLightPosition.x, -10.0f, 10.0f);
    ImGui::SliderFloat("スポット Y", &lighting.spotLightPosition.y, -10.0f, 10.0f);
    ImGui::SliderFloat("スポット Z", &lighting.spotLightPosition.z, -10.0f, 10.0f);
    ImGui::SliderFloat("スポット向き X", &lighting.spotLightDirection.x, -1.0f, 1.0f);
    ImGui::SliderFloat("スポット向き Y", &lighting.spotLightDirection.y, -1.0f, 1.0f);
    ImGui::SliderFloat("スポット向き Z", &lighting.spotLightDirection.z, -1.0f, 1.0f);
    ImGui::SliderFloat("スポットの強さ", &lighting.spotLightIntensity, 0.0f, 20.0f);
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

void ImGuiManager::ShowGameView(
    D3D12_GPU_DESCRIPTOR_HANDLE textureHandle,
    const Math::Vector2& textureSize,
    bool& isOpen,
    bool isPlayMode,
    bool& requestStartPlayMode,
    bool& requestStopPlayMode,
    int& viewportTabIndex,
    bool isSceneCameraEnabled,
    ObjectInspectorSettings& inspector)
{
#ifdef USE_IMGUI
    if (!isOpen) {
        return;
    }

    ImGui::SetNextWindowPos(
        ImVec2(kEditorCenterX, kEditorPanelTop),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        ImVec2(kEditorCenterWidth, kEditorViewportHeight),
        ImGuiCond_Always);
    if (!ImGui::Begin(
        "ビューポート",
        &isOpen,
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove)) {
        ImGui::End();
        return;
    }

    auto drawViewportImage = [&]() {
        ImVec2 availableSize = ImGui::GetContentRegionAvail();
        const float aspect =
            textureSize.y > 0.0f ? textureSize.x / textureSize.y : 1.0f;
        float imageWidth = availableSize.x;
        float imageHeight = imageWidth / aspect;
        if (imageHeight > availableSize.y) {
            imageHeight = availableSize.y;
            imageWidth = imageHeight * aspect;
        }

        const float offsetX = (availableSize.x - imageWidth) * 0.5f;
        if (offsetX > 0.0f) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
        }
        ImGui::Image(
            static_cast<ImTextureID>(textureHandle.ptr),
            ImVec2(imageWidth, imageHeight));
    };

    if (ImGui::BeginTabBar("ViewportTabs")) {
        const ImGuiTabItemFlags sceneTabFlags =
            viewportTabIndex == 0 ? ImGuiTabItemFlags_SetSelected : 0;
        if (ImGui::BeginTabItem("シーン", nullptr, sceneTabFlags)) {
            viewportTabIndex = 0;
            ImGui::TextUnformatted(
                isSceneCameraEnabled ?
                "シーンカメラ: ON" :
                "シーンカメラ: OFF");
            ImGui::Separator();
            drawViewportImage();
            ImGui::EndTabItem();
        }

        const ImGuiTabItemFlags gameTabFlags =
            viewportTabIndex == 1 ? ImGuiTabItemFlags_SetSelected : 0;
        if (ImGui::BeginTabItem("ゲーム", nullptr, gameTabFlags)) {
            viewportTabIndex = 1;
            if (isPlayMode) {
                if (ImGui::Button("停止##ViewportStop")) {
                    requestStopPlayMode = true;
                }
                ImGui::SameLine();
                ImGui::TextUnformatted("再生モード");
            } else {
                if (ImGui::Button("再生##ViewportPlay")) {
                    requestStartPlayMode = true;
                }
                ImGui::SameLine();
                ImGui::TextUnformatted("編集プレビュー");
            }
            ImGui::Separator();
            drawViewportImage();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();

    ImGui::SetNextWindowPos(
        ImVec2(kEditorCenterX, kEditorProjectY),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        ImVec2(kEditorCenterWidth, kEditorProjectHeight),
        ImGuiCond_Always);
    ImGui::Begin(
        "プロジェクト",
        nullptr,
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove);

    if (ImGui::BeginTabBar("ProjectTabs")) {
        if (ImGui::BeginTabItem("プロジェクト")) {
            static char projectFilter[64] = {};
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint(
                "##ProjectSearch",
                "検索",
                projectFilter,
                sizeof(projectFilter));
            if (!inspectorStatus_.empty()) {
                ImGui::TextWrapped("選択中: %s", inspectorStatus_.c_str());
            }
            ImGui::Separator();

            ImGui::Columns(4, "ProjectColumns", false);
            ImGui::TextUnformatted("シーン");
            ImGui::BeginDisabled(isPlayMode);
            for (int index = 0; index < inspector.sceneFileItemCount; ++index) {
                if (!MatchesProjectFilter(
                    inspector.sceneFileItems[index],
                    projectFilter)) {
                    continue;
                }
                const bool isSelected = inspector.sceneFileIndex == index;
                if (ImGui::Selectable(
                    inspector.sceneFileItems[index],
                    isSelected)) {
                    inspector.sceneFileIndex = index;
                    inspector.requestLoadObjects = true;
                    inspectorStatus_ = std::string("シーン読み込み: ") +
                        inspector.sceneFileItems[index];
                }
            }
            ImGui::EndDisabled();
            ImGui::NextColumn();

            ImGui::TextUnformatted("プレハブ");
            ImGui::BeginDisabled(isPlayMode);
            for (int index = 0; index < inspector.prefabFileItemCount; ++index) {
                if (!MatchesProjectFilter(
                    inspector.prefabFileItems[index],
                    projectFilter)) {
                    continue;
                }
                const bool isSelected = inspector.prefabFileIndex == index;
                if (ImGui::Selectable(
                    inspector.prefabFileItems[index],
                    isSelected)) {
                    inspector.prefabFileIndex = index;
                    inspector.requestInstantiatePrefab = true;
                    inspectorStatus_ = std::string("プレハブ生成: ") +
                        inspector.prefabFileItems[index];
                }
            }
            ImGui::EndDisabled();
            ImGui::NextColumn();

            ImGui::TextUnformatted("モデル");
            ImGui::BeginDisabled(isPlayMode);
            for (int index = 0; index < IM_ARRAYSIZE(kProjectModelAssets); ++index) {
                if (!MatchesProjectFilter(kProjectModelAssets[index], projectFilter)) {
                    continue;
                }
                const bool isSelected = inspector.addModelIndex == index;
                if (ImGui::Selectable(kProjectModelAssets[index], isSelected)) {
                    inspector.addModelIndex = index;
                    inspector.requestAddObject = true;
                    inspectorStatus_ = std::string("モデル追加: ") +
                        kProjectModelAssets[index];
                }
            }
            ImGui::EndDisabled();
            ImGui::NextColumn();

            ImGui::TextUnformatted("テクスチャ");
            Object3d* selectedObject = nullptr;
            if (0 <= inspector.selectedObjectIndex &&
                inspector.selectedObjectIndex < inspector.objectCount) {
                selectedObject =
                    inspector.objects[inspector.selectedObjectIndex].object;
            }
            ImGui::BeginDisabled(isPlayMode || !selectedObject);
            for (const char* asset : kProjectTextureAssets) {
                if (!MatchesProjectFilter(asset, projectFilter)) {
                    continue;
                }
                const bool isSelected =
                    selectedObject &&
                    selectedObject->GetTextureFilePath() == asset;
                if (ImGui::Selectable(asset, isSelected)) {
                    selectedObject->SetTextureFilePath(asset);
                    inspectorStatus_ = std::string("テクスチャ適用: ") + asset;
                }
            }
            ImGui::EndDisabled();
            ImGui::Columns(1);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("コンソール")) {
            static char consoleFilter[64] = {};
            static bool showInfoLogs = true;
            static bool showOperationLogs = true;

            ImGui::SetNextItemWidth(220.0f);
            ImGui::InputTextWithHint(
                "##ConsoleSearch",
                "検索",
                consoleFilter,
                sizeof(consoleFilter));
            ImGui::SameLine();
            ImGui::Checkbox("情報", &showInfoLogs);
            ImGui::SameLine();
            ImGui::Checkbox("操作", &showOperationLogs);
            ImGui::SameLine();
            if (ImGui::Button("クリア")) {
                inspectorStatus_.clear();
            }
            ImGui::Separator();

            auto showConsoleLine = [&](const char* type, const char* message) {
                if (!MatchesProjectFilter(message, consoleFilter) &&
                    !MatchesProjectFilter(type, consoleFilter)) {
                    return;
                }
                ImGui::Text("[%s] %s", type, message);
            };

            if (showInfoLogs) {
                showConsoleLine("情報", "エディターレイアウト準備完了");
                showConsoleLine("情報", "F1: エディター表示切り替え");
                showConsoleLine("情報", "F2: 再生モード切り替え");
                showConsoleLine("情報", "F4: 再生中のゲームデバッグ切り替え");
            }
            if (showOperationLogs && !inspectorStatus_.empty()) {
                showConsoleLine("操作", inspectorStatus_.c_str());
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
#else
    (void)textureHandle;
    (void)textureSize;
    (void)isOpen;
    (void)isPlayMode;
    (void)requestStartPlayMode;
    (void)requestStopPlayMode;
    (void)viewportTabIndex;
    (void)isSceneCameraEnabled;
    (void)inspector;
#endif
}
