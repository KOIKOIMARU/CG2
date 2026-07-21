#include "engine/scene/BonusShowcaseScene.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>

#include "engine/3d/Camera.h"
#include "engine/3d/ModelManager.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/3d/Skybox.h"
#include "engine/base/DirectXCommon.h"
#include "engine/io/Input.h"
#include "engine/scene/SceneManager.h"
#include "engine/scene/SceneType.h"

namespace {

constexpr const char* kEnvironmentTexturePath =
    "resources/skybox/kloofendal_48d_partly_cloudy_puresky_4k_cube.dds";
constexpr Math::Vector3 kBoneDebugOffset = { 1.75f, 0.0f, 0.0f };

Math::Vector3 ExtractTranslation(const Math::Matrix4x4& matrix)
{
    return {
        matrix.m[3][0],
        matrix.m[3][1],
        matrix.m[3][2]
    };
}

Math::Vector3 MakeBoneRotate(const Math::Vector3& direction)
{
    const float horizontalLength =
        std::sqrt(direction.x * direction.x + direction.z * direction.z);
    return {
        std::atan2(-direction.y, horizontalLength),
        std::atan2(direction.x, direction.z),
        0.0f
    };
}

} // namespace

BonusShowcaseScene::BonusShowcaseScene() = default;
BonusShowcaseScene::~BonusShowcaseScene() = default;

void BonusShowcaseScene::Initialize()
{
    object3dCommon_ = std::make_unique<Object3dCommon>();
    object3dCommon_->Initialize(dxCommon_, srvManager_);
    object3dCommon_->SetEnvironmentTexturePath(kEnvironmentTexturePath);

    camera_ = std::make_unique<Camera>();
    camera_->SetRotate({ 0.08f, 0.0f, 0.0f });
    camera_->SetTranslate({ 0.0f, 2.2f, -13.5f });
    camera_->SetFovY(0.52f);
    object3dCommon_->SetDefaultCamera(camera_.get());

    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(
        dxCommon_,
        srvManager_,
        kEnvironmentTexturePath);

    ModelManager::GetInstance()->Initialize(dxCommon_, srvManager_);
    ModelManager::GetInstance()->SetEnvironmentTexturePath(
        kEnvironmentTexturePath);
    ModelManager::GetInstance()->CreateSphere(
        "bonus_joint_sphere",
        10,
        18,
        1.0f,
        "resources/human/white.png");
    ModelManager::GetInstance()->CreateBox(
        "bonus_bone_box",
        1.0f,
        1.0f,
        1.0f,
        "resources/human/white.png");
    ModelManager::GetInstance()->LoadModel("simpleSkin/simpleSkin.gltf");
    ModelManager::GetInstance()->LoadModel("human/walk.gltf");
    ModelManager::GetInstance()->LoadModel("multiMesh.obj");
    ModelManager::GetInstance()->LoadModel("multiMaterial.obj");

    simpleSkinObject_ = CreateDisplayObject(
        "simpleSkin/simpleSkin.gltf",
        { -4.45f, 0.0f, 0.0f },
        { 0.82f, 0.82f, 0.82f });
    simpleSkinObject_->SetLightingMode(1);
    simpleSkinObject_->SetEnvironmentCoefficient(0.0f);

    humanWalkObject_ = CreateDisplayObject(
        "human/walk.gltf",
        { -2.25f, 0.0f, 0.0f },
        { 1.05f, 1.05f, 1.05f });

    multiMeshObject_ = CreateDisplayObject(
        "multiMesh.obj",
        { 2.05f, 0.55f, 0.0f },
        { 0.72f, 0.72f, 0.72f });
    multiMeshObject_->SetRotate({ 0.0f, 0.35f, 0.0f });

    multiMaterialObject_ = CreateDisplayObject(
        "multiMaterial.obj",
        { 4.45f, 0.55f, 0.0f },
        { 0.72f, 0.72f, 0.72f });
    multiMaterialObject_->SetRotate({ 0.0f, -0.35f, 0.0f });

    InitializeBoneDebug();
    camera_->Update();
}

void BonusShowcaseScene::Finalize()
{
    jointDebugObjects_.clear();
    boneDebugObjects_.clear();
    multiMaterialObject_.reset();
    multiMeshObject_.reset();
    humanWalkObject_.reset();
    simpleSkinObject_.reset();
    skybox_.reset();
    camera_.reset();
    object3dCommon_.reset();
}

void BonusShowcaseScene::Update()
{
    if (input_ &&
        (input_->TriggerKey(DIK_F2) || input_->TriggerKey(DIK_ESCAPE))) {
        if (sceneManager_) {
            sceneManager_->SetNextScene(SceneType::Title);
        }
        return;
    }
    if (input_ && input_->TriggerKey(DIK_B)) {
        showBoneDebug_ = !showBoneDebug_;
    }

    const float deltaTime = dxCommon_ ? dxCommon_->GetDeltaTime() : 0.0f;
    demonstrationTime_ += deltaTime;

    camera_->Update();
    skybox_->Update(camera_.get());
    simpleSkinObject_->UpdateAnimation(deltaTime);
    humanWalkObject_->UpdateAnimation(deltaTime);

    const float turn = demonstrationTime_ * 0.35f;
    multiMeshObject_->SetRotate({ 0.0f, 0.35f + turn, 0.0f });
    multiMaterialObject_->SetRotate({ 0.0f, -0.35f - turn, 0.0f });

    simpleSkinObject_->Update();
    humanWalkObject_->Update();
    multiMeshObject_->Update();
    multiMaterialObject_->Update();
    UpdateBoneDebug();
    DrawShowcaseHud();
}

void BonusShowcaseScene::Draw()
{
    skybox_->Draw();
    object3dCommon_->CommonDrawSetting();
    simpleSkinObject_->Draw();
    humanWalkObject_->Draw();
    multiMeshObject_->Draw();
    multiMaterialObject_->Draw();

    if (!showBoneDebug_) {
        return;
    }

    for (auto& bone : boneDebugObjects_) {
        bone->Draw();
    }
    for (auto& joint : jointDebugObjects_) {
        joint->Draw();
    }
}

std::unique_ptr<Object3d> BonusShowcaseScene::CreateDisplayObject(
    const char* modelName,
    const Math::Vector3& translate,
    const Math::Vector3& scale)
{
    auto object = std::make_unique<Object3d>();
    object->Initialize(object3dCommon_.get());
    object->SetModel(modelName);
    object->SetTranslate(translate);
    object->SetScale(scale);
    object->SetLightingMode(2);
    object->SetEnvironmentCoefficient(0.12f);
    object->SetShadowReceiveStrength(0.0f);
    object->Update();
    return object;
}

void BonusShowcaseScene::InitializeBoneDebug()
{
    jointDebugObjects_.clear();
    boneDebugObjects_.clear();
    if (!humanWalkObject_ || !humanWalkObject_->HasSkeleton()) {
        return;
    }

    const Skeleton& skeleton = humanWalkObject_->GetSkeleton();
    jointDebugObjects_.reserve(skeleton.joints.size());
    boneDebugObjects_.reserve(skeleton.joints.size());
    for (const Joint& joint : skeleton.joints) {
        auto jointObject = CreateDisplayObject(
            "bonus_joint_sphere",
            { 0.0f, -100.0f, 0.0f },
            { 0.020f, 0.020f, 0.020f });
        jointObject->SetLightingMode(0);
        jointObject->SetEnvironmentCoefficient(0.0f);
        jointObject->SetColor({ 0.28f, 0.95f, 1.0f, 1.0f });
        jointDebugObjects_.push_back(std::move(jointObject));

        if (joint.parent.has_value()) {
            auto boneObject = CreateDisplayObject(
            "bonus_bone_box",
            { 0.0f, -100.0f, 0.0f },
            { 0.012f, 0.012f, 0.012f });
            boneObject->SetLightingMode(0);
            boneObject->SetEnvironmentCoefficient(0.0f);
            boneObject->SetColor({ 1.0f, 0.72f, 0.18f, 1.0f });
            boneDebugObjects_.push_back(std::move(boneObject));
        }
    }
}

void BonusShowcaseScene::UpdateBoneDebug()
{
    if (!humanWalkObject_ || !humanWalkObject_->HasSkeleton()) {
        return;
    }

    const Skeleton& skeleton = humanWalkObject_->GetSkeleton();
    const Math::Matrix4x4& worldMatrix = humanWalkObject_->GetWorldMatrix();
    for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex) {
        const Math::Vector3 position = ExtractTranslation(Math::Multiply(
            skeleton.joints[jointIndex].skeletonSpaceMatrix,
            worldMatrix));
        jointDebugObjects_[jointIndex]->SetTranslate({
            position.x + kBoneDebugOffset.x,
            position.y + kBoneDebugOffset.y,
            position.z + kBoneDebugOffset.z
        });
        jointDebugObjects_[jointIndex]->Update();
    }

    size_t boneIndex = 0;
    for (const Joint& joint : skeleton.joints) {
        if (!joint.parent.has_value()) {
            continue;
        }

        const Math::Vector3 jointPosition = ExtractTranslation(Math::Multiply(
            joint.skeletonSpaceMatrix,
            worldMatrix));
        const Math::Vector3 parentPosition = ExtractTranslation(Math::Multiply(
            skeleton.joints[*joint.parent].skeletonSpaceMatrix,
            worldMatrix));
        const Math::Vector3 difference{
            jointPosition.x - parentPosition.x,
            jointPosition.y - parentPosition.y,
            jointPosition.z - parentPosition.z
        };
        const float length = std::sqrt(
            difference.x * difference.x +
            difference.y * difference.y +
            difference.z * difference.z);
        const float safeLength = (std::max)(length, 0.001f);
        const Math::Vector3 direction{
            difference.x / safeLength,
            difference.y / safeLength,
            difference.z / safeLength
        };
        const Math::Vector3 center{
            (jointPosition.x + parentPosition.x) * 0.5f + kBoneDebugOffset.x,
            (jointPosition.y + parentPosition.y) * 0.5f + kBoneDebugOffset.y,
            (jointPosition.z + parentPosition.z) * 0.5f + kBoneDebugOffset.z
        };

        Object3d* boneObject = boneDebugObjects_[boneIndex].get();
        boneObject->SetTranslate(center);
        boneObject->SetRotate(MakeBoneRotate(direction));
        boneObject->SetScale({ 0.012f, 0.012f, safeLength });
        boneObject->Update();
        ++boneIndex;
    }
}

void BonusShowcaseScene::DrawShowcaseHud()
{
    ImGui::SetNextWindowPos(ImVec2(24.0f, 24.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(410.0f, 220.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.90f);
    ImGui::Begin(
        "Bonus Feature Showcase",
        nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    ImGui::TextColored(
        ImVec4(0.35f, 0.92f, 1.0f, 1.0f),
        "F1 BONUS SHOWCASE / 50 POINTS");
    ImGui::Separator();
    ImGui::BulletText("Skinning model display (20)");
    ImGui::BulletText("Compute Shader skinning (10)");
    ImGui::BulletText("MultiMesh + MultiMaterial (5)");
    ImGui::BulletText("Animation interpolation: Lerp / Slerp (5)");
    ImGui::BulletText("Animated bone debug display (10)");
    ImGui::Separator();
    ImGui::Text("B: bone display %s", showBoneDebug_ ? "ON" : "OFF");
    ImGui::TextUnformatted("F2 / Esc: return to title");
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(24.0f, 630.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.78f);
    ImGui::Begin(
        "Bonus Labels",
        nullptr,
        ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoInputs);
    ImGui::TextUnformatted(
        "SKIN+COMPUTE    ANIM MODEL    BONE DEBUG    MULTI MESH    MULTI MATERIAL");
    ImGui::End();
}
