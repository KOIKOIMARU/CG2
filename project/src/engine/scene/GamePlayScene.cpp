#include "engine/scene/GamePlayScene.h"

#include "engine/3d/Object3dCommon.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Model.h"
#include "engine/3d/Camera.h"
#include "engine/2d/Sprite.h"
#include "engine/2d/SpriteCommon.h"
#include "engine/3d/TextureManager.h"
#include "engine/3d/ParticleManager.h"
#include "engine/3d/ParticleEmitter.h"
#include "engine/3d/Skybox.h"
#include "engine/base/ImGuiManager.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/SrvManager.h"
#include "engine/scene/SceneSerializer.h"
#include "engine/3d/ModelManager.h"
#include "engine/io/Input.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

namespace {

constexpr const char* kInspectorModelItems[] = {
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

constexpr const char* kSceneFilePath =
"resources/scene_debug_transforms.json";
constexpr const char* kEditorSettingsPath =
"resources/editor_settings.json";
constexpr size_t kMaxTransformHistoryCount = 64;
constexpr float kTransformHistoryEpsilon = 0.0001f;

bool IsSameVector3(const Math::Vector3& lhs, const Math::Vector3& rhs)
{
    return std::fabs(lhs.x - rhs.x) <= kTransformHistoryEpsilon &&
        std::fabs(lhs.y - rhs.y) <= kTransformHistoryEpsilon &&
        std::fabs(lhs.z - rhs.z) <= kTransformHistoryEpsilon;
}

bool ExtractSettingsInt(const std::string& source, const char* key, int& out)
{
    const std::regex pattern(
        std::string("\"") + key + "\"\\s*:\\s*(-?[0-9]+)"
    );
    std::smatch match;
    if (!std::regex_search(source, match, pattern)) {
        return false;
    }

    out = std::stoi(match[1].str());
    return true;
}

bool ExtractSettingsFloat(
    const std::string& source,
    const char* key,
    float& out)
{
    const std::regex pattern(
        std::string("\"") + key + "\"\\s*:\\s*([-+0-9.eE]+)"
    );
    std::smatch match;
    if (!std::regex_search(source, match, pattern)) {
        return false;
    }

    out = std::stof(match[1].str());
    return true;
}

bool ExtractSettingsBool(const std::string& source, const char* key, bool& out)
{
    const std::regex pattern(
        std::string("\"") + key + "\"\\s*:\\s*(true|false)"
    );
    std::smatch match;
    if (!std::regex_search(source, match, pattern)) {
        return false;
    }

    out = match[1].str() == "true";
    return true;
}

bool ExtractSettingsVector3(
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

Math::Vector3 Add(const Math::Vector3& a, const Math::Vector3& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

Math::Vector3 Subtract(const Math::Vector3& a, const Math::Vector3& b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

Math::Vector3 MakeForwardVector(const Math::Vector3& rotate)
{
    const float cosPitch = std::cos(rotate.x);
    const float sinPitch = std::sin(rotate.x);
    const float cosYaw = std::cos(rotate.y);
    const float sinYaw = std::sin(rotate.y);

    return Math::Normalize({
        sinYaw * cosPitch,
        -sinPitch,
        cosYaw * cosPitch
    });
}

Math::Vector3 MakeRightVector(const Math::Vector3& rotate)
{
    const float yaw = rotate.y + (3.14159265358979323846f * 0.5f);
    return Math::Normalize({
        std::sin(yaw),
        0.0f,
        std::cos(yaw)
    });
}

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

GamePlayScene::GamePlayScene() = default;
GamePlayScene::~GamePlayScene() = default;


void GamePlayScene::Initialize() {
    const std::string environmentTexturePath =
        "resources/skybox/kloofendal_48d_partly_cloudy_puresky_4k_cube.dds";

    object3dCommon_ = std::make_unique<Object3dCommon>();
    object3dCommon_->Initialize(dxCommon_, srvManager_);
    object3dCommon_->SetEnvironmentTexturePath(environmentTexturePath);

    camera_ = std::make_unique<Camera>();
    camera_->SetRotate({ 0.2f, 0.0f, 0.0f });
    camera_->SetTranslate({ 0.0f, 6.0f, -24.0f });
    object3dCommon_->SetDefaultCamera(camera_.get());

    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(
        dxCommon_,
        srvManager_,
        environmentTexturePath
    );

    ModelManager::GetInstance()->Initialize(dxCommon_, srvManager_);
    ModelManager::GetInstance()->SetEnvironmentTexturePath(
        environmentTexturePath
    );
    ModelManager::GetInstance()->CreatePlane(
        "primitive_plane",
        8.0f,
        8.0f,
        "resources/checkerBoard.png"
    );
    ModelManager::GetInstance()->CreateTriangle(
        "primitive_triangle",
        1.6f,
        1.6f,
        "resources/uvChecker.png"
    );
    ModelManager::GetInstance()->CreateCircle(
        "primitive_circle",
        32,
        0.9f,
        "resources/uvChecker.png"
    );
    ModelManager::GetInstance()->CreateRing(
        "primitive_ring",
        32,
        2.0f,
        1.0f,
        "resources/gradationLine.png"
    );
    ModelManager::GetInstance()->CreateSphere(
        "primitive_sphere",
        16,
        32,
        1.0f,
        "resources/uvChecker.png"
    );
    ModelManager::GetInstance()->CreateSphere(
        "debug_joint_sphere",
        16,
        32,
        1.0f,
        "resources/human/white.png"
    );
    ModelManager::GetInstance()->CreateTorus(
        "primitive_torus",
        32,
        16,
        0.8f,
        0.3f,
        "resources/uvChecker.png"
    );
    ModelManager::GetInstance()->CreateCylinder(
        "primitive_cylinder",
        32,
        1.2f,
        1.2f,
        2.5f,
        "resources/gradationLine.png"
    );
    ModelManager::GetInstance()->CreateCone(
        "primitive_cone",
        32,
        0.8f,
        1.6f,
        "resources/uvChecker.png"
    );
    ModelManager::GetInstance()->CreateBox(
        "primitive_box",
        1.5f,
        1.5f,
        1.5f,
        "resources/uvChecker.png"
    );
    ModelManager::GetInstance()->CreateBox(
        "debug_bone_box",
        1.0f,
        1.0f,
        1.0f,
        "resources/human/white.png"
    );
    ModelManager::GetInstance()->LoadModel("AnimatedCube/AnimatedCube.gltf");
    ModelManager::GetInstance()->LoadModel("simpleSkin/simpleSkin.gltf");
    ModelManager::GetInstance()->LoadModel("human/sneakWalk.gltf");
    ModelManager::GetInstance()->LoadModel("human/walk.gltf");

    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCommon_.get());
    object3d_->SetModel("primitive_plane");
    object3d_->SetEnvironmentCoefficient(environmentCoefficient_);
    object3d_->SetRotate({ 1.5707963f, 0.0f, 0.0f });

    ringObject_ = std::make_unique<Object3d>();
    ringObject_->Initialize(object3dCommon_.get());
    ringObject_->SetModel("primitive_ring");
    ringObject_->SetTranslate({ 0.0f, 0.05f, 0.0f });
    ringObject_->SetRotate({ 1.5707963f, 0.0f, 0.0f });
    ringObject_->SetEnvironmentCoefficient(0.0f);

    cylinderObject_ = std::make_unique<Object3d>();
    cylinderObject_->Initialize(object3dCommon_.get());
    cylinderObject_->SetModel("primitive_cylinder");
    cylinderObject_->SetTranslate({ 0.0f, 0.0f, 0.0f });
    cylinderObject_->SetEnvironmentCoefficient(0.0f);
    cylinderObject_->SetLightingMode(0);
    cylinderObject_->SetColor(cylinderColor_);
    cylinderObject_->SetAlphaReference(cylinderAlphaReference_);

    sphereObject_ = std::make_unique<Object3d>();
    sphereObject_->Initialize(object3dCommon_.get());
    sphereObject_->SetModel("primitive_sphere");
    sphereObject_->SetScale({ 1.0f, 1.0f, 1.0f });
    sphereObject_->SetTranslate({ 0.0f, 1.5f, 3.0f });
    sphereObject_->SetEnvironmentCoefficient(environmentCoefficient_);

    animatedCubeObject_ = std::make_unique<Object3d>();
    animatedCubeObject_->Initialize(object3dCommon_.get());
    animatedCubeObject_->SetModel("AnimatedCube/AnimatedCube.gltf");
    animatedCubeObject_->SetTranslate({ 0.0f, 1.5f, -3.5f });
    animatedCubeObject_->SetScale({ 1.0f, 1.0f, 1.0f });
    animatedCubeObject_->SetEnvironmentCoefficient(0.0f);

    simpleSkinObject_ = std::make_unique<Object3d>();
    simpleSkinObject_->Initialize(object3dCommon_.get());
    simpleSkinObject_->SetModel("simpleSkin/simpleSkin.gltf");
    simpleSkinObject_->SetTranslate({ -7.0f, 0.0f, 0.0f });
    simpleSkinObject_->SetEnvironmentCoefficient(0.0f);

    humanSneakObject_ = std::make_unique<Object3d>();
    humanSneakObject_->Initialize(object3dCommon_.get());
    humanSneakObject_->SetModel("human/sneakWalk.gltf");
    humanSneakObject_->SetTranslate({ 0.0f, 0.0f, 0.0f });
    humanSneakObject_->SetEnvironmentCoefficient(0.0f);

    humanWalkObject_ = std::make_unique<Object3d>();
    humanWalkObject_->Initialize(object3dCommon_.get());
    humanWalkObject_->SetModel("human/walk.gltf");
    humanWalkObject_->SetTranslate({ 7.0f, 0.0f, 0.0f });
    humanWalkObject_->SetEnvironmentCoefficient(0.0f);

    InitializeSkeletonDebugSet(
        simpleSkinDebug_,
        simpleSkinObject_.get(),
        { 1.0f, 1.0f, 1.0f, 1.0f }
    );
    InitializeSkeletonDebugSet(
        humanSneakDebug_,
        humanSneakObject_.get(),
        { 1.0f, 1.0f, 1.0f, 1.0f }
    );
    InitializeSkeletonDebugSet(
        humanWalkDebug_,
        humanWalkObject_.get(),
        { 1.0f, 1.0f, 1.0f, 1.0f }
    );

    struct PrimitiveSample {
        const char* modelName;
        Math::Vector3 position;
        int modelIndex;
    };
    const PrimitiveSample primitiveSamples[] = {
        { "primitive_triangle", { -4.5f, 1.0f, 3.0f }, 4 },
        { "primitive_circle",   { -2.0f, 1.0f, 3.0f }, 5 },
        { "primitive_box",      { 2.5f, 1.0f, 3.0f }, 6 },
        { "primitive_torus",    { 5.0f, 1.2f, 3.0f }, 7 },
        { "primitive_cone",     { -5.0f, 1.0f, -2.0f }, 8 },
    };

    for (const auto& sample : primitiveSamples) {
        auto primitiveObject = std::make_unique<Object3d>();
        primitiveObject->Initialize(object3dCommon_.get());
        primitiveObject->SetModel(sample.modelName);
        primitiveObject->SetTranslate(sample.position);
        primitiveObject->SetEnvironmentCoefficient(0.0f);

        EditorObject editorObject{};
        editorObject.name = sample.modelName;
        editorObject.modelIndex = sample.modelIndex;
        editorObject.object = std::move(primitiveObject);
        editorObjects_.push_back(std::move(editorObject));
    }

    editorManager_.SetSceneFilePath(kSceneFilePath);
    LoadEditorSettings();
    LoadPrimitiveObjectsFromSceneFile(editorManager_.GetSceneFilePath());

    TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
    TextureManager::GetInstance()->LoadTexture("resources/monsterBall.png");
    TextureManager::GetInstance()->LoadTexture("resources/checkerBoard.png");
    TextureManager::GetInstance()->LoadTexture("resources/gradationLine.png");
    TextureManager::GetInstance()->LoadTexture("resources/circle2.png");

    ParticleManager::GetInstance()->CreateParticleGroup(
        "test",
        "resources/circle2.png"
    );

    emitter_ = std::make_unique<ParticleEmitter>(
        "test",
        Math::Vector3{ 0.0f, 2.0f, 0.0f },
        0.5f,
        10
    );
}

bool GamePlayScene::LoadPrimitiveObjectsFromSceneFile(const char* path) {
    std::vector<SceneSerializer::ObjectRecord> records;
    SceneSerializer::SceneSettings settings{};
    if (!SceneSerializer::LoadScene(path, records, settings)) {
        return false;
    }

    ApplySceneSettings(settings);
    ClearTransformHistory();

    struct BaseObjectBinding {
        const char* name;
        Object3d* object;
        int modelIndexSlot;
    };
    const BaseObjectBinding baseObjects[] = {
        { "Plane", object3d_.get(), 0 },
        { "Ring", ringObject_.get(), 1 },
        { "Cylinder", cylinderObject_.get(), 2 },
        { "Sphere", sphereObject_.get(), 3 },
        { "Animated Cube", animatedCubeObject_.get(), 4 },
        { "Simple Skin", simpleSkinObject_.get(), 5 },
        { "Human Sneak", humanSneakObject_.get(), 6 },
        { "Human Walk", humanWalkObject_.get(), 7 }
    };
    const int modelItemCount =
        static_cast<int>(
            sizeof(kInspectorModelItems) / sizeof(kInspectorModelItems[0]));
    for (const BaseObjectBinding& binding : baseObjects) {
        if (!binding.object) {
            continue;
        }

        const SceneSerializer::ObjectRecord* record =
            SceneSerializer::FindObjectByName(records, binding.name);
        if (!record) {
            continue;
        }

        if (0 <= record->modelIndex && record->modelIndex < modelItemCount) {
            binding.object->SetModel(kInspectorModelItems[record->modelIndex]);
            inspectObjectModelIndices_[binding.modelIndexSlot] =
                record->modelIndex;
        }
        binding.object->SetTranslate(record->translate);
        binding.object->SetRotate(record->rotate);
        binding.object->SetScale(record->scale);
        binding.object->SetColor(record->color);
        binding.object->SetAlphaReference(record->alphaReference);
        binding.object->SetLightingMode(record->lightingMode);
        if (!record->textureFilePath.empty()) {
            binding.object->SetTextureFilePath(record->textureFilePath);
        }
    }

    editorObjects_.clear();

    for (const SceneSerializer::ObjectRecord& record : records) {
        if (!record.primitive) {
            continue;
        }

        if (record.modelIndex < 0 || modelItemCount <= record.modelIndex) {
            continue;
        }

        auto primitiveObject = std::make_unique<Object3d>();
        primitiveObject->Initialize(object3dCommon_.get());
        primitiveObject->SetModel(kInspectorModelItems[record.modelIndex]);
        primitiveObject->SetTranslate(record.translate);
        primitiveObject->SetRotate(record.rotate);
        primitiveObject->SetScale(record.scale);
        primitiveObject->SetColor(record.color);
        primitiveObject->SetAlphaReference(record.alphaReference);
        primitiveObject->SetLightingMode(record.lightingMode);
        if (!record.textureFilePath.empty()) {
            primitiveObject->SetTextureFilePath(record.textureFilePath);
        }
        primitiveObject->SetEnvironmentCoefficient(0.0f);

        EditorObject editorObject{};
        editorObject.name = record.name.empty() ?
            kInspectorModelItems[record.modelIndex] :
            record.name;
        editorObject.modelIndex = record.modelIndex;
        editorObject.object = std::move(primitiveObject);
        editorObjects_.push_back(std::move(editorObject));
    }

    editorManager_.ResetSelectedObjectIndex();
    return true;
}

bool GamePlayScene::SaveSceneToFile(const char* path) const
{
    return SceneSerializer::SaveScene(
        path,
        BuildSceneObjectRecords(),
        BuildSceneSettings());
}

void GamePlayScene::AddEditorPrimitive(int modelIndex)
{
    const int modelItemCount =
        static_cast<int>(
            sizeof(kInspectorModelItems) / sizeof(kInspectorModelItems[0]));

    if (modelIndex < 0 || modelIndex >= modelItemCount) {
        return;
    }

    auto primitiveObject = std::make_unique<Object3d>();
    primitiveObject->Initialize(object3dCommon_.get());
    primitiveObject->SetModel(kInspectorModelItems[modelIndex]);
    primitiveObject->SetTranslate({
        0.0f,
        1.0f,
        static_cast<float>(editorObjects_.size()) + 5.0f
        });
    primitiveObject->SetEnvironmentCoefficient(0.0f);

    EditorObject editorObject{};
    editorObject.name =
        "Added Primitive " + std::to_string(editorObjects_.size() + 1);
    editorObject.modelIndex = modelIndex;
    editorObject.object = std::move(primitiveObject);

    editorObjects_.push_back(std::move(editorObject));
    ClearTransformHistory();
}

void GamePlayScene::RemoveSelectedEditorObject()
{
    constexpr int kBaseInspectObjectCount = 8;

    const int primitiveIndex =
        editorManager_.GetSelectedObjectIndex() - kBaseInspectObjectCount;

    if (primitiveIndex < 0 ||
        primitiveIndex >= static_cast<int>(editorObjects_.size())) {
        return;
    }

    editorObjects_.erase(editorObjects_.begin() + primitiveIndex);
    editorManager_.ResetSelectedObjectIndex();
    ClearTransformHistory();
}

void GamePlayScene::DuplicateSelectedEditorObject()
{
    constexpr int kBaseInspectObjectCount = 8;

    const int primitiveIndex =
        editorManager_.GetSelectedObjectIndex() - kBaseInspectObjectCount;

    if (primitiveIndex < 0 ||
        primitiveIndex >= static_cast<int>(editorObjects_.size())) {
        return;
    }

    const EditorObject& source = editorObjects_[primitiveIndex];
    auto duplicatedObject = std::make_unique<Object3d>();
    duplicatedObject->Initialize(object3dCommon_.get());
    duplicatedObject->SetModel(kInspectorModelItems[source.modelIndex]);

    Math::Vector3 translate = source.object->GetTranslate();
    translate.x += 1.0f;
    translate.z += 1.0f;
    duplicatedObject->SetTranslate(translate);
    duplicatedObject->SetRotate(source.object->GetRotate());
    duplicatedObject->SetScale(source.object->GetScale());
    duplicatedObject->SetColor(source.object->GetColor());
    duplicatedObject->SetAlphaReference(source.object->GetAlphaReference());
    duplicatedObject->SetLightingMode(source.object->GetLightingMode());
    duplicatedObject->SetTextureFilePath(source.object->GetTextureFilePath());
    duplicatedObject->SetEnvironmentCoefficient(
        source.object->GetEnvironmentCoefficient());

    EditorObject duplicatedEditorObject{};
    duplicatedEditorObject.name = source.name + " Copy";
    duplicatedEditorObject.modelIndex = source.modelIndex;
    duplicatedEditorObject.object = std::move(duplicatedObject);
    editorObjects_.push_back(std::move(duplicatedEditorObject));
    ClearTransformHistory();
}

void GamePlayScene::SaveSelectedEditorObjectAsPrefab()
{
    constexpr int kBaseInspectObjectCount = 8;

    const int primitiveIndex =
        editorManager_.GetSelectedObjectIndex() - kBaseInspectObjectCount;

    if (primitiveIndex < 0 ||
        primitiveIndex >= static_cast<int>(editorObjects_.size())) {
        return;
    }

    const EditorObject& source = editorObjects_[primitiveIndex];
    SceneSerializer::ObjectRecord record{};
    record.name = source.name;
    record.primitive = true;
    record.modelIndex = source.modelIndex;
    record.translate = source.object->GetTranslate();
    record.rotate = source.object->GetRotate();
    record.scale = source.object->GetScale();
    record.color = source.object->GetColor();
    record.alphaReference = source.object->GetAlphaReference();
    record.lightingMode = source.object->GetLightingMode();
    record.textureFilePath = source.object->GetTextureFilePath();

    SceneSerializer::SaveObjects(editorManager_.GetPrefabFilePath(), { record });
}

void GamePlayScene::InstantiatePrefab()
{
    std::vector<SceneSerializer::ObjectRecord> records;
    if (!SceneSerializer::LoadObjects(editorManager_.GetPrefabFilePath(), records) ||
        records.empty()) {
        return;
    }

    const SceneSerializer::ObjectRecord& record = records.front();
    const int modelItemCount =
        static_cast<int>(
            sizeof(kInspectorModelItems) / sizeof(kInspectorModelItems[0]));
    if (record.modelIndex < 0 || modelItemCount <= record.modelIndex) {
        return;
    }

    auto object = std::make_unique<Object3d>();
    object->Initialize(object3dCommon_.get());
    object->SetModel(kInspectorModelItems[record.modelIndex]);
    Math::Vector3 translate = record.translate;
    translate.x += 1.5f;
    translate.z += 1.5f;
    object->SetTranslate(translate);
    object->SetRotate(record.rotate);
    object->SetScale(record.scale);
    object->SetColor(record.color);
    object->SetAlphaReference(record.alphaReference);
    object->SetLightingMode(record.lightingMode);
    if (!record.textureFilePath.empty()) {
        object->SetTextureFilePath(record.textureFilePath);
    }
    object->SetEnvironmentCoefficient(0.0f);

    EditorObject editorObject{};
    editorObject.name = record.name.empty() ?
        "Prefab Instance" :
        record.name + " Instance";
    editorObject.modelIndex = record.modelIndex;
    editorObject.object = std::move(object);
    editorObjects_.push_back(std::move(editorObject));
    ClearTransformHistory();
}

void GamePlayScene::BuildInspectableObjects(
    std::vector<ImGuiManager::InspectableObject>& inspectObjects)
{
    inspectObjects = {
        { "Plane", ImGuiManager::InspectableType::Object3d, object3d_.get(), &inspectObjectModelIndices_[0], nullptr },
        { "Ring", ImGuiManager::InspectableType::Object3d, ringObject_.get(), &inspectObjectModelIndices_[1], nullptr },
        { "Cylinder", ImGuiManager::InspectableType::Object3d, cylinderObject_.get(), &inspectObjectModelIndices_[2], nullptr },
        { "Sphere", ImGuiManager::InspectableType::Object3d, sphereObject_.get(), &inspectObjectModelIndices_[3], nullptr },
        { "Animated Cube", ImGuiManager::InspectableType::Object3d, animatedCubeObject_.get(), &inspectObjectModelIndices_[4], nullptr },
        { "Simple Skin", ImGuiManager::InspectableType::Object3d, simpleSkinObject_.get(), &inspectObjectModelIndices_[5], nullptr },
        { "Human Sneak", ImGuiManager::InspectableType::Object3d, humanSneakObject_.get(), &inspectObjectModelIndices_[6], nullptr },
        { "Human Walk", ImGuiManager::InspectableType::Object3d, humanWalkObject_.get(), &inspectObjectModelIndices_[7], nullptr }
    };

    for (auto& editorObject : editorObjects_) {
        inspectObjects.push_back({
     editorObject.name.c_str(),
     ImGuiManager::InspectableType::Object3d,
     editorObject.object.get(),
     &editorObject.modelIndex,
     &editorObject.name
            });
    }
}

std::vector<SceneSerializer::ObjectRecord> GamePlayScene::BuildSceneObjectRecords() const
{
    std::vector<SceneSerializer::ObjectRecord> records;

    auto appendObject =
        [&records](
            const char* name,
            const Object3d* object,
            bool primitive,
            int modelIndex) {
            if (!object) {
                return;
            }

            SceneSerializer::ObjectRecord record{};
            record.name = name ? name : "";
            record.primitive = primitive;
            record.modelIndex = modelIndex;
            record.translate = object->GetTranslate();
            record.rotate = object->GetRotate();
            record.scale = object->GetScale();
            record.color = object->GetColor();
            record.alphaReference = object->GetAlphaReference();
            record.lightingMode = object->GetLightingMode();
            record.textureFilePath = object->GetTextureFilePath();
            records.push_back(record);
        };

    appendObject("Plane", object3d_.get(), false, inspectObjectModelIndices_[0]);
    appendObject("Ring", ringObject_.get(), false, inspectObjectModelIndices_[1]);
    appendObject(
        "Cylinder",
        cylinderObject_.get(),
        false,
        inspectObjectModelIndices_[2]);
    appendObject("Sphere", sphereObject_.get(), false, inspectObjectModelIndices_[3]);
    appendObject(
        "Animated Cube",
        animatedCubeObject_.get(),
        false,
        inspectObjectModelIndices_[4]);
    appendObject(
        "Simple Skin",
        simpleSkinObject_.get(),
        false,
        inspectObjectModelIndices_[5]);
    appendObject(
        "Human Sneak",
        humanSneakObject_.get(),
        false,
        inspectObjectModelIndices_[6]);
    appendObject(
        "Human Walk",
        humanWalkObject_.get(),
        false,
        inspectObjectModelIndices_[7]);

    for (const EditorObject& editorObject : editorObjects_) {
        appendObject(
            editorObject.name.c_str(),
            editorObject.object.get(),
            true,
            editorObject.modelIndex);
    }

    return records;
}

SceneSerializer::SceneSettings GamePlayScene::BuildSceneSettings() const
{
    SceneSerializer::SceneSettings settings{};
    if (camera_) {
        settings.hasCamera = true;
        settings.cameraTranslate = camera_->GetTranslate();
        settings.cameraRotate = camera_->GetRotate();
    }

    settings.hasLighting = true;
    settings.lightDirection = lightDirection_;
    settings.lightIntensity = lightIntensity_;
    settings.pointLightPosition = pointLightPosition_;
    settings.pointLightIntensity = pointLightIntensity_;
    settings.spotLightPosition = spotLightPosition_;
    settings.spotLightDirection = spotLightDirection_;
    settings.spotLightIntensity = spotLightIntensity_;
    return settings;
}

void GamePlayScene::ApplySceneSettings(
    const SceneSerializer::SceneSettings& settings)
{
    if (settings.hasCamera && camera_) {
        camera_->SetTranslate(settings.cameraTranslate);
        camera_->SetRotate(settings.cameraRotate);
    }

    if (settings.hasLighting) {
        lightDirection_ = settings.lightDirection;
        lightIntensity_ = settings.lightIntensity;
        pointLightPosition_ = settings.pointLightPosition;
        pointLightIntensity_ = settings.pointLightIntensity;
        spotLightPosition_ = settings.spotLightPosition;
        spotLightDirection_ = settings.spotLightDirection;
        spotLightIntensity_ = settings.spotLightIntensity;
    }
}

void GamePlayScene::ProcessEditorRequests()
{
    if (editorManager_.IsPlayMode()) {
        return;
    }

    if (editorManager_.ConsumeUndoRequest() && !undoStack_.empty()) {
        const TransformHistoryRecord record = undoStack_.back();
        undoStack_.pop_back();
        ApplyTransformHistory(record, false);
        redoStack_.push_back(record);
        lastTransformHistoryObjectIndex_ = -1;
    }

    if (editorManager_.ConsumeRedoRequest() && !redoStack_.empty()) {
        const TransformHistoryRecord record = redoStack_.back();
        redoStack_.pop_back();
        ApplyTransformHistory(record, true);
        undoStack_.push_back(record);
        lastTransformHistoryObjectIndex_ = -1;
    }

    if (editorManager_.ConsumeSaveObjectsRequest()) {
        SaveSceneToFile(editorManager_.GetSceneFilePath());
    }

    if (editorManager_.ConsumeLoadObjectsRequest()) {
        LoadPrimitiveObjectsFromSceneFile(editorManager_.GetSceneFilePath());
    }

    if (editorManager_.ConsumeRemoveObjectRequest()) {
        RemoveSelectedEditorObject();
    }

    if (editorManager_.ConsumeDuplicateObjectRequest()) {
        DuplicateSelectedEditorObject();
    }

    if (editorManager_.ConsumeSavePrefabRequest()) {
        SaveSelectedEditorObjectAsPrefab();
    }

    if (editorManager_.ConsumeInstantiatePrefabRequest()) {
        InstantiatePrefab();
    }

    if (editorManager_.ConsumeAddObjectRequest()) {
        AddEditorPrimitive(editorManager_.GetAddModelIndex());
    }
}

void GamePlayScene::TrackTransformHistory(
    const std::vector<ImGuiManager::InspectableObject>& inspectObjects)
{
    if (editorManager_.IsPlayMode()) {
        return;
    }

    const int selectedIndex = editorManager_.GetSelectedObjectIndex();
    if (selectedIndex < 0 ||
        selectedIndex >= static_cast<int>(inspectObjects.size())) {
        lastTransformHistoryObjectIndex_ = -1;
        return;
    }

    Object3d* object = inspectObjects[selectedIndex].object;
    if (!object) {
        lastTransformHistoryObjectIndex_ = -1;
        return;
    }

    const Math::Vector3 translate = object->GetTranslate();
    const Math::Vector3 rotate = object->GetRotate();
    const Math::Vector3 scale = object->GetScale();

    if (lastTransformHistoryObjectIndex_ != selectedIndex) {
        lastTransformHistoryObjectIndex_ = selectedIndex;
        lastObservedTranslate_ = translate;
        lastObservedRotate_ = rotate;
        lastObservedScale_ = scale;
        return;
    }

    if (IsSameVector3(lastObservedTranslate_, translate) &&
        IsSameVector3(lastObservedRotate_, rotate) &&
        IsSameVector3(lastObservedScale_, scale)) {
        return;
    }

    TransformHistoryRecord record{};
    record.object = object;
    record.beforeTranslate = lastObservedTranslate_;
    record.beforeRotate = lastObservedRotate_;
    record.beforeScale = lastObservedScale_;
    record.afterTranslate = translate;
    record.afterRotate = rotate;
    record.afterScale = scale;
    undoStack_.push_back(record);
    if (undoStack_.size() > kMaxTransformHistoryCount) {
        undoStack_.erase(undoStack_.begin());
    }
    redoStack_.clear();

    lastObservedTranslate_ = translate;
    lastObservedRotate_ = rotate;
    lastObservedScale_ = scale;
}

void GamePlayScene::ClearTransformHistory()
{
    undoStack_.clear();
    redoStack_.clear();
    lastTransformHistoryObjectIndex_ = -1;
}

void GamePlayScene::ApplyTransformHistory(
    const TransformHistoryRecord& record,
    bool useAfter)
{
    if (!record.object) {
        return;
    }

    record.object->SetTranslate(
        useAfter ? record.afterTranslate : record.beforeTranslate);
    record.object->SetRotate(
        useAfter ? record.afterRotate : record.beforeRotate);
    record.object->SetScale(
        useAfter ? record.afterScale : record.beforeScale);
}

void GamePlayScene::ApplyLightingToObject(Object3d* object)
{
    if (!object) {
        return;
    }

    object->SetDirectionalLightDirection(lightDirection_);
    object->SetDirectionalLightIntensity(lightIntensity_);
    object->SetPointLightPosition(pointLightPosition_);
    object->SetPointLightIntensity(pointLightIntensity_);
    object->SetSpotLightPosition(spotLightPosition_);
    object->SetSpotLightDirection(spotLightDirection_);
    object->SetSpotLightIntensity(spotLightIntensity_);
}

void GamePlayScene::UpdateAnimations(float deltaTime)
{
    if (Model* animatedCubeModel =
        ModelManager::GetInstance()->FindModel("AnimatedCube/AnimatedCube.gltf")) {
        const Animation& animation = animatedCubeModel->GetAnimation();

        if (animation.duration > 0.0f &&
            animatedCubeModel->HasAnimation()) {
            animationTime_ += deltaTime * animation.ticksPerSecond;

            while (animationTime_ > animation.duration) {
                animationTime_ -= animation.duration;
            }

            const std::string& rootNodeName =
                animatedCubeModel->GetRootNode().name;
            auto it = animation.nodeAnimations.find(rootNodeName);

            if (it != animation.nodeAnimations.end()) {
                const QuaternionTransform transform =
                    Model::CalculateValue(it->second, animationTime_);

                animatedCubeObject_->SetScale(transform.scale);
                animatedCubeObject_->SetQuaternionRotate(transform.rotate);
                animatedCubeObject_->SetTranslate({
                    transform.translate.x,
                    transform.translate.y + 1.5f,
                    transform.translate.z - 3.5f
                    });
            }
        }
    }

    ApplyLightingToObject(animatedCubeObject_.get());

    if (simpleSkinObject_) {
        simpleSkinObject_->UpdateAnimation(deltaTime);
        ApplyLightingToObject(simpleSkinObject_.get());
    }

    if (humanSneakObject_) {
        humanSneakObject_->UpdateAnimation(deltaTime);
        ApplyLightingToObject(humanSneakObject_.get());
    }

    if (humanWalkObject_) {
        humanWalkObject_->UpdateAnimation(deltaTime);
        ApplyLightingToObject(humanWalkObject_.get());
    }
}

void GamePlayScene::Update() {
    const float deltaTime = dxCommon_->GetDeltaTime();

    if (input_ && input_->TriggerKey(DIK_F1)) {
        isDebugCameraEnabled_ = !isDebugCameraEnabled_;
    }

    if (isDebugCameraEnabled_) {
        UpdateDebugCamera(deltaTime);
    }

    imguiManager_->ShowSpriteController(spritePos_);

    std::vector<ImGuiManager::InspectableObject> inspectObjects;
    BuildInspectableObjects(inspectObjects);

    const int inspectObjectCount = static_cast<int>(inspectObjects.size());
    editorManager_.ValidateSelectedObjectIndex(inspectObjectCount);

    ImGuiManager::GamePlayDebugSettings debugSettings{
        {
            blendModeIndex_,
            environmentCoefficient_,
            showSkybox_,
            postEffectMode_
        },
        {
            objectRotate_,
            showPlane_,
            showRing_,
            showCylinder_,
            showSphere_,
            showParticle_
        },
        {
            cylinderColor_,
            cylinderAlphaReference_,
            cylinderUVScrollSpeed_
        },
        {
            lightDirection_,
            lightIntensity_,
            pointLightPosition_,
            pointLightIntensity_,
            spotLightPosition_,
            spotLightDirection_,
            spotLightIntensity_
        },
        {
            renderingPanelOpen_,
            objectsPanelOpen_,
            inspectorPanelOpen_,
            materialPanelOpen_,
            cylinderPanelOpen_,
            lightingPanelOpen_
        },
        editorManager_.CreateInspectorSettings(
            inspectObjects.data(),
            inspectObjectCount)
    };
    imguiManager_->ShowGamePlayController(debugSettings);

    TrackTransformHistory(inspectObjects);
    ProcessEditorRequests();

    for (auto& sprite : sprites_) {
        sprite.SetPosition(spritePos_);
    }
    object3d_->SetRotate(objectRotate_);
    ApplyLightingToObject(object3d_.get());

    ringObject_->SetRotate({ 1.5707963f, objectRotate_.y, objectRotate_.z });
    ApplyLightingToObject(ringObject_.get());

    cylinderUVOffset_ += cylinderUVScrollSpeed_ * deltaTime;
    Matrix4x4 cylinderUVTransform = Multiply(
        MakeScaleMatrix({ 1.0f, -1.0f, 1.0f }),
        MakeTranslateMatrix({ cylinderUVOffset_, 1.0f, 0.0f })
    );
    cylinderObject_->SetColor(cylinderColor_);
    cylinderObject_->SetAlphaReference(cylinderAlphaReference_);
    cylinderObject_->SetUVTransform(cylinderUVTransform);
    ApplyLightingToObject(cylinderObject_.get());

    sphereObject_->SetRotate(objectRotate_);
    ApplyLightingToObject(sphereObject_.get());

    UpdateAnimations(deltaTime);

    if (showSkeletonDebug_) {
        UpdateSkeletonDebugSet(simpleSkinDebug_);
        UpdateSkeletonDebugSet(humanSneakDebug_);
        UpdateSkeletonDebugSet(humanWalkDebug_);
    }

    for (auto& editorObject : editorObjects_) {
        editorObject.object->SetRotate(objectRotate_);
        ApplyLightingToObject(editorObject.object.get());
    }

    object3dCommon_->SetBlendMode(
        static_cast<BlendMode>(blendModeIndex_));

    camera_->Update();
    skybox_->Update(camera_.get());
    object3d_->Update();
    ringObject_->Update();
    cylinderObject_->Update();
    sphereObject_->Update();
    animatedCubeObject_->Update();
    if (simpleSkinObject_) {
        simpleSkinObject_->Update();
    }
    if (humanSneakObject_) {
        humanSneakObject_->Update();
    }
    if (humanWalkObject_) {
        humanWalkObject_->Update();
    }
    if (showSkeletonDebug_) {
        for (auto& joint : simpleSkinDebug_.joints) {
            joint->Update();
        }
        for (auto& bone : simpleSkinDebug_.bones) {
            bone->Update();
        }
        for (auto& joint : humanSneakDebug_.joints) {
            joint->Update();
        }
        for (auto& bone : humanSneakDebug_.bones) {
            bone->Update();
        }
        for (auto& joint : humanWalkDebug_.joints) {
            joint->Update();
        }
        for (auto& bone : humanWalkDebug_.bones) {
            bone->Update();
        }
    }
    for (auto& editorObject : editorObjects_) {
        editorObject.object->Update();
    }

    if (emitter_) {
        emitter_->Update(deltaTime);
    }

    ParticleManager::GetInstance()->Update(
        camera_->GetViewMatrix(),
        camera_->GetProjectionMatrix()
    );

    for (auto& s : sprites_) {
        s.Update();
    }
}

void GamePlayScene::LoadEditorSettings()
{
    std::ifstream file(kEditorSettingsPath);
    if (!file) {
        return;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string json = buffer.str();

    int sceneFileIndex = 0;
    int prefabFileIndex = 0;
    int gizmoMode = 0;
    ExtractSettingsInt(json, "postEffectMode", postEffectMode_);
    ExtractSettingsBool(json, "showSkybox", showSkybox_);
    ExtractSettingsFloat(json, "environmentCoefficient", environmentCoefficient_);
    ExtractSettingsVector3(json, "lightDirection", lightDirection_);
    ExtractSettingsFloat(json, "lightIntensity", lightIntensity_);
    ExtractSettingsVector3(json, "pointLightPosition", pointLightPosition_);
    ExtractSettingsFloat(json, "pointLightIntensity", pointLightIntensity_);
    ExtractSettingsVector3(json, "spotLightPosition", spotLightPosition_);
    ExtractSettingsVector3(json, "spotLightDirection", spotLightDirection_);
    ExtractSettingsFloat(json, "spotLightIntensity", spotLightIntensity_);
    ExtractSettingsBool(json, "renderingPanelOpen", renderingPanelOpen_);
    ExtractSettingsBool(json, "objectsPanelOpen", objectsPanelOpen_);
    ExtractSettingsBool(json, "inspectorPanelOpen", inspectorPanelOpen_);
    ExtractSettingsBool(json, "materialPanelOpen", materialPanelOpen_);
    ExtractSettingsBool(json, "cylinderPanelOpen", cylinderPanelOpen_);
    ExtractSettingsBool(json, "lightingPanelOpen", lightingPanelOpen_);

    if (ExtractSettingsInt(json, "sceneFileIndex", sceneFileIndex)) {
        editorManager_.SetSceneFileIndex(sceneFileIndex);
    }
    if (ExtractSettingsInt(json, "prefabFileIndex", prefabFileIndex)) {
        editorManager_.SetPrefabFileIndex(prefabFileIndex);
    }
    if (ExtractSettingsInt(json, "gizmoMode", gizmoMode)) {
        editorManager_.SetGizmoMode(gizmoMode);
    }
}

void GamePlayScene::SaveEditorSettings() const
{
    std::filesystem::path path(kEditorSettingsPath);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream file(path);
    if (!file) {
        return;
    }

    file << "{\n";
    file << "  \"postEffectMode\": " << postEffectMode_ << ",\n";
    file << "  \"showSkybox\": " << (showSkybox_ ? "true" : "false") << ",\n";
    file << "  \"environmentCoefficient\": " << environmentCoefficient_ << ",\n";
    file << "  \"lightDirection\": [" << lightDirection_.x << ", "
        << lightDirection_.y << ", " << lightDirection_.z << "],\n";
    file << "  \"lightIntensity\": " << lightIntensity_ << ",\n";
    file << "  \"pointLightPosition\": [" << pointLightPosition_.x << ", "
        << pointLightPosition_.y << ", " << pointLightPosition_.z << "],\n";
    file << "  \"pointLightIntensity\": " << pointLightIntensity_ << ",\n";
    file << "  \"spotLightPosition\": [" << spotLightPosition_.x << ", "
        << spotLightPosition_.y << ", " << spotLightPosition_.z << "],\n";
    file << "  \"spotLightDirection\": [" << spotLightDirection_.x << ", "
        << spotLightDirection_.y << ", " << spotLightDirection_.z << "],\n";
    file << "  \"spotLightIntensity\": " << spotLightIntensity_ << ",\n";
    file << "  \"renderingPanelOpen\": "
        << (renderingPanelOpen_ ? "true" : "false") << ",\n";
    file << "  \"objectsPanelOpen\": "
        << (objectsPanelOpen_ ? "true" : "false") << ",\n";
    file << "  \"inspectorPanelOpen\": "
        << (inspectorPanelOpen_ ? "true" : "false") << ",\n";
    file << "  \"materialPanelOpen\": "
        << (materialPanelOpen_ ? "true" : "false") << ",\n";
    file << "  \"cylinderPanelOpen\": "
        << (cylinderPanelOpen_ ? "true" : "false") << ",\n";
    file << "  \"lightingPanelOpen\": "
        << (lightingPanelOpen_ ? "true" : "false") << ",\n";
    file << "  \"sceneFileIndex\": " << editorManager_.GetSceneFileIndex() << ",\n";
    file << "  \"prefabFileIndex\": " << editorManager_.GetPrefabFileIndex() << ",\n";
    file << "  \"gizmoMode\": " << editorManager_.GetGizmoMode() << "\n";
    file << "}\n";
}

void GamePlayScene::UpdateDebugCamera(float deltaTime)
{
    if (!input_ || !camera_) {
        return;
    }

    Math::Vector3 rotate = camera_->GetRotate();
    Math::Vector3 translate = camera_->GetTranslate();

    const float rotateStep = debugCameraRotateSpeed_ * deltaTime;
    if (input_->PushKey(DIK_LEFT)) {
        rotate.y -= rotateStep;
    }
    if (input_->PushKey(DIK_RIGHT)) {
        rotate.y += rotateStep;
    }
    if (input_->PushKey(DIK_UP)) {
        rotate.x -= rotateStep;
    }
    if (input_->PushKey(DIK_DOWN)) {
        rotate.x += rotateStep;
    }

    rotate.x = std::clamp(rotate.x, -1.4f, 1.4f);

    const Math::Vector3 forward = MakeForwardVector(rotate);
    const Math::Vector3 right = MakeRightVector(rotate);
    const Math::Vector3 up = { 0.0f, 1.0f, 0.0f };

    Math::Vector3 move = { 0.0f, 0.0f, 0.0f };
    if (input_->PushKey(DIK_W)) {
        move = Add(move, forward);
    }
    if (input_->PushKey(DIK_S)) {
        move = Subtract(move, forward);
    }
    if (input_->PushKey(DIK_D)) {
        move = Add(move, right);
    }
    if (input_->PushKey(DIK_A)) {
        move = Subtract(move, right);
    }
    if (input_->PushKey(DIK_SPACE)) {
        move = Add(move, up);
    }
    if (input_->PushKey(DIK_LSHIFT) || input_->PushKey(DIK_RSHIFT)) {
        move = Subtract(move, up);
    }

    if (move.x != 0.0f || move.y != 0.0f || move.z != 0.0f) {
        move = Math::Normalize(move) * (debugCameraMoveSpeed_ * deltaTime);
        translate = Add(translate, move);
    }

    camera_->SetRotate(rotate);
    camera_->SetTranslate(translate);
}

void GamePlayScene::Draw() {
    if (showSkybox_) {
        skybox_->Draw();
    }

    object3dCommon_->CommonDrawSetting();
    if (showPlane_) {
        object3d_->Draw();
    }
    if (showRing_) {
        ringObject_->Draw();
    }
    if (showCylinder_) {
        cylinderObject_->Draw();
    }
    if (showSphere_) {
        sphereObject_->Draw();
        animatedCubeObject_->Draw();
        for (auto& editorObject : editorObjects_) {
            editorObject.object->Draw();
        }
    }

    if (showSkinningSamples_) {
        if (simpleSkinObject_) {
            simpleSkinObject_->Draw();
        }
        if (humanSneakObject_) {
            humanSneakObject_->Draw();
        }
        if (humanWalkObject_) {
            humanWalkObject_->Draw();
        }
    }

    if (showSkinningSamples_ && showSkeletonDebug_) {
        object3dCommon_->SetDepthDrawMode(DepthDrawMode::Overlay);
        object3dCommon_->CommonDrawSetting();
        for (auto& bone : simpleSkinDebug_.bones) {
            bone->Draw();
        }
        for (auto& joint : simpleSkinDebug_.joints) {
            joint->Draw();
        }
        for (auto& bone : humanSneakDebug_.bones) {
            bone->Draw();
        }
        for (auto& joint : humanSneakDebug_.joints) {
            joint->Draw();
        }
        for (auto& bone : humanWalkDebug_.bones) {
            bone->Draw();
        }
        for (auto& joint : humanWalkDebug_.joints) {
            joint->Draw();
        }
        object3dCommon_->SetDepthDrawMode(DepthDrawMode::Normal);
        object3dCommon_->CommonDrawSetting();
    }

    if (showParticle_) {
        ParticleManager::GetInstance()->Draw();
    }

    spriteCommon_->CommonDrawSetting();
    for (auto& s : sprites_) {
        s.Draw();
    }
}

void GamePlayScene::Finalize() {
    SaveEditorSettings();

    emitter_.reset();
    cylinderObject_.reset();
    ringObject_.reset();
    sphereObject_.reset();
    animatedCubeObject_.reset();
    simpleSkinObject_.reset();
    humanSneakObject_.reset();
    humanWalkObject_.reset();
    simpleSkinDebug_.joints.clear();
    simpleSkinDebug_.bones.clear();
    humanSneakDebug_.joints.clear();
    humanSneakDebug_.bones.clear();
    humanWalkDebug_.joints.clear();
    humanWalkDebug_.bones.clear();
    editorObjects_.clear();
    object3d_.reset();
    skybox_.reset();
    camera_.reset();
    object3dCommon_.reset();

    ModelManager::GetInstance()->Finalize();
}

void GamePlayScene::InitializeSkeletonDebugSet(
    SkeletonDebugSet& debugSet,
    Object3d* source,
    const Math::Vector4& color)
{
    debugSet.source = source;
    debugSet.color = color;
    debugSet.joints.clear();
    debugSet.bones.clear();

    if (!source || !source->HasSkeleton()) {
        return;
    }

    const Skeleton& skeleton = source->GetSkeleton();
    debugSet.joints.reserve(skeleton.joints.size());
    debugSet.bones.reserve(skeleton.joints.size());

    for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex) {
        auto jointObject = std::make_unique<Object3d>();
        jointObject->Initialize(object3dCommon_.get());
        jointObject->SetModel("debug_joint_sphere");
        jointObject->SetScale({ 0.006f, 0.006f, 0.006f });
        jointObject->SetLightingMode(0);
        jointObject->SetEnvironmentCoefficient(0.0f);
        jointObject->SetColor(color);
        debugSet.joints.push_back(std::move(jointObject));

        if (skeleton.joints[jointIndex].parent.has_value()) {
            auto boneObject = std::make_unique<Object3d>();
            boneObject->Initialize(object3dCommon_.get());
            boneObject->SetModel("debug_bone_box");
            boneObject->SetLightingMode(0);
            boneObject->SetEnvironmentCoefficient(0.0f);
            boneObject->SetColor(color);
            debugSet.bones.push_back(std::move(boneObject));
        }
    }
}

void GamePlayScene::UpdateSkeletonDebugSet(SkeletonDebugSet& debugSet)
{
    if (!debugSet.source || !debugSet.source->HasSkeleton()) {
        return;
    }

    const bool isSimpleSkin = debugSet.source == simpleSkinObject_.get();
    const Skeleton& skeleton = debugSet.source->GetSkeleton();
    const Matrix4x4& worldMatrix = debugSet.source->GetWorldMatrix();

    for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex) {
        const Matrix4x4 jointWorldMatrix = Multiply(
            skeleton.joints[jointIndex].skeletonSpaceMatrix,
            worldMatrix
        );
        const Math::Vector3 jointPosition = ExtractTranslation(jointWorldMatrix);

        debugSet.joints[jointIndex]->SetTranslate(jointPosition);
        if (isSimpleSkin) {
            debugSet.joints[jointIndex]->SetScale({ 0.02f, 0.02f, 0.02f });
            if (jointIndex == 0) {
                debugSet.joints[jointIndex]->SetColor({ 1.0f, 0.55f, 0.75f, 1.0f });
            } else {
                debugSet.joints[jointIndex]->SetColor({ 0.55f, 0.9f, 1.0f, 1.0f });
            }
        } else {
            debugSet.joints[jointIndex]->SetScale({ 0.006f, 0.006f, 0.006f });
            debugSet.joints[jointIndex]->SetColor(debugSet.color);
        }
    }

    size_t boneIndex = 0;
    for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex) {
        const auto& joint = skeleton.joints[jointIndex];
        if (!joint.parent.has_value()) {
            continue;
        }

        const Matrix4x4 jointWorldMatrix = Multiply(
            joint.skeletonSpaceMatrix,
            worldMatrix
        );
        const Matrix4x4 parentWorldMatrix = Multiply(
            skeleton.joints[*joint.parent].skeletonSpaceMatrix,
            worldMatrix
        );

        const Math::Vector3 jointPosition = ExtractTranslation(jointWorldMatrix);
        const Math::Vector3 parentPosition = ExtractTranslation(parentWorldMatrix);
        const Math::Vector3 diff = Subtract(jointPosition, parentPosition);
        const float length =
            std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

        if (length <= 0.0001f) {
            if (isSimpleSkin) {
                debugSet.bones[boneIndex]->SetScale({ 0.0015f, 0.0015f, 0.0015f });
            } else {
                debugSet.bones[boneIndex]->SetScale({ 0.002f, 0.002f, 0.002f });
            }
            debugSet.bones[boneIndex]->SetTranslate(parentPosition);
            debugSet.bones[boneIndex]->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
            ++boneIndex;
            continue;
        }

        const Math::Vector3 direction = {
            diff.x / length,
            diff.y / length,
            diff.z / length
        };
        const Math::Vector3 center = {
            (jointPosition.x + parentPosition.x) * 0.5f,
            (jointPosition.y + parentPosition.y) * 0.5f,
            (jointPosition.z + parentPosition.z) * 0.5f
        };

        if (isSimpleSkin) {
            debugSet.bones[boneIndex]->SetScale({ 0.0025f, 0.0025f, length });
        } else {
            debugSet.bones[boneIndex]->SetScale({ 0.006f, 0.006f, length });
        }
        debugSet.bones[boneIndex]->SetRotate(MakeBoneRotate(direction));
        debugSet.bones[boneIndex]->SetTranslate(center);
        debugSet.bones[boneIndex]->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        ++boneIndex;
    }
}
