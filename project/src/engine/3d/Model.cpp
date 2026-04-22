#include "engine/3d/Model.h"

#include <fstream>
#include <sstream>
#include <cassert>
#include <filesystem>
#include <numbers>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "engine/3d/ModelCommon.h"
#include "engine/base/DirectXCommon.h"
#include "engine/3d/TextureManager.h"
#include "engine/base/SrvManager.h"

namespace {

Matrix4x4 ConvertAiMatrixToMatrix4x4(aiMatrix4x4 matrix)
{
    matrix.Transpose();

    Matrix4x4 result{};
    result.m[0][0] = matrix.a1;
    result.m[0][1] = matrix.a2;
    result.m[0][2] = matrix.a3;
    result.m[0][3] = matrix.a4;
    result.m[1][0] = matrix.b1;
    result.m[1][1] = matrix.b2;
    result.m[1][2] = matrix.b3;
    result.m[1][3] = matrix.b4;
    result.m[2][0] = matrix.c1;
    result.m[2][1] = matrix.c2;
    result.m[2][2] = matrix.c3;
    result.m[2][3] = matrix.c4;
    result.m[3][0] = matrix.d1;
    result.m[3][1] = matrix.d2;
    result.m[3][2] = matrix.d3;
    result.m[3][3] = matrix.d4;
    return result;
}

Vector4 TransformPosition(const Vector4& position, const Matrix4x4& matrix)
{
    return {
        position.x * matrix.m[0][0] +
            position.y * matrix.m[1][0] +
            position.z * matrix.m[2][0] +
            position.w * matrix.m[3][0],
        position.x * matrix.m[0][1] +
            position.y * matrix.m[1][1] +
            position.z * matrix.m[2][1] +
            position.w * matrix.m[3][1],
        position.x * matrix.m[0][2] +
            position.y * matrix.m[1][2] +
            position.z * matrix.m[2][2] +
            position.w * matrix.m[3][2],
        position.x * matrix.m[0][3] +
            position.y * matrix.m[1][3] +
            position.z * matrix.m[2][3] +
            position.w * matrix.m[3][3],
    };
}

Vector3 TransformNormal(const Vector3& normal, const Matrix4x4& matrix)
{
    Vector4 transformed =
        TransformPosition({ normal.x, normal.y, normal.z, 0.0f }, matrix);
    return Normalize({ transformed.x, transformed.y, transformed.z });
}

Node ReadNode(aiNode* node)
{
    assert(node);

    aiVector3D scale;
    aiQuaternion rotate;
    aiVector3D translate;
    node->mTransformation.Decompose(scale, rotate, translate);

    Node result;
    result.name = node->mName.C_Str();
    result.transform.scale = { scale.x, scale.y, scale.z };
    result.transform.rotate = { rotate.x, rotate.y, rotate.z, rotate.w };
    result.transform.translate = { translate.x, translate.y, translate.z };
    result.localMatrix = MakeAffineMatrix(
        result.transform.scale,
        result.transform.rotate,
        result.transform.translate
    );

    result.children.resize(node->mNumChildren);
    for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
        result.children[childIndex] = ReadNode(node->mChildren[childIndex]);
    }

    return result;
}

Animation ReadAnimation(const aiScene* scene)
{
    Animation animation;
    if (!scene || scene->mNumAnimations == 0) {
        return animation;
    }

    const aiAnimation* aiAnimation = scene->mAnimations[0];
    assert(aiAnimation);

    animation.duration = static_cast<float>(aiAnimation->mDuration);
    animation.ticksPerSecond =
        aiAnimation->mTicksPerSecond == 0.0 ?
        1.0f :
        static_cast<float>(aiAnimation->mTicksPerSecond);

    for (uint32_t channelIndex = 0; channelIndex < aiAnimation->mNumChannels; ++channelIndex) {
        const aiNodeAnim* nodeAnimation = aiAnimation->mChannels[channelIndex];
        NodeAnimation animationChannel;

        for (uint32_t keyIndex = 0; keyIndex < nodeAnimation->mNumPositionKeys; ++keyIndex) {
            const auto& key = nodeAnimation->mPositionKeys[keyIndex];
            animationChannel.translate.push_back({
                static_cast<float>(key.mTime),
                { key.mValue.x, key.mValue.y, key.mValue.z }
            });
        }

        for (uint32_t keyIndex = 0; keyIndex < nodeAnimation->mNumRotationKeys; ++keyIndex) {
            const auto& key = nodeAnimation->mRotationKeys[keyIndex];
            animationChannel.rotate.push_back({
                static_cast<float>(key.mTime),
                { key.mValue.x, key.mValue.y, key.mValue.z, key.mValue.w }
            });
        }

        for (uint32_t keyIndex = 0; keyIndex < nodeAnimation->mNumScalingKeys; ++keyIndex) {
            const auto& key = nodeAnimation->mScalingKeys[keyIndex];
            animationChannel.scale.push_back({
                static_cast<float>(key.mTime),
                { key.mValue.x, key.mValue.y, key.mValue.z }
            });
        }

        animation.nodeAnimations[nodeAnimation->mNodeName.C_Str()] =
            animationChannel;
    }

    return animation;
}

std::string GetAssimpTexturePath(
    const aiScene* scene,
    const std::string& directoryPath)
{
    for (uint32_t i = 0; i < scene->mNumMaterials; ++i) {
        aiMaterial* material = scene->mMaterials[i];
        aiString texturePath;

        if (material->GetTexture(aiTextureType_BASE_COLOR, 0, &texturePath) ==
                AI_SUCCESS ||
            material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) ==
                AI_SUCCESS) {
            std::filesystem::path path = texturePath.C_Str();
            return directoryPath + "/" + path.filename().string();
        }
    }

    return directoryPath + "/uvChecker.png";
}

void AppendAssimpNode(
    const aiScene* scene,
    const aiNode* node,
    const Matrix4x4& parentMatrix,
    ModelData& modelData)
{
    Matrix4x4 localMatrix =
        ConvertAiMatrixToMatrix4x4(node->mTransformation);
    Matrix4x4 worldMatrix = Multiply(localMatrix, parentMatrix);

    for (uint32_t meshIndex = 0; meshIndex < node->mNumMeshes; ++meshIndex) {
        const aiMesh* mesh = scene->mMeshes[node->mMeshes[meshIndex]];
        assert(mesh);
        assert(mesh->HasNormals());

        for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
            const aiFace& face = mesh->mFaces[faceIndex];
            assert(face.mNumIndices == 3);

            for (uint32_t index = 0; index < face.mNumIndices; ++index) {
                uint32_t vertexIndex = face.mIndices[index];
                const aiVector3D& position = mesh->mVertices[vertexIndex];
                const aiVector3D& normal = mesh->mNormals[vertexIndex];

                Vector2 texcoord{ 0.0f, 0.0f };
                if (mesh->HasTextureCoords(0)) {
                    const aiVector3D& uv =
                        mesh->mTextureCoords[0][vertexIndex];
                    texcoord = { uv.x, uv.y };
                }

                VertexData vertex{};
                vertex.position = TransformPosition(
                    { position.x, position.y, position.z, 1.0f },
                    worldMatrix);
                vertex.texcoord = texcoord;
                vertex.normal = TransformNormal(
                    { normal.x, normal.y, normal.z },
                    worldMatrix);

                modelData.vertices.push_back(vertex);
            }
        }
    }

    for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
        AppendAssimpNode(
            scene,
            node->mChildren[childIndex],
            worldMatrix,
            modelData);
    }
}

void AppendTriangle(
    ModelData& modelData,
    const VertexData& v0,
    const VertexData& v1,
    const VertexData& v2)
{
    modelData.vertices.push_back(v0);
    modelData.vertices.push_back(v1);
    modelData.vertices.push_back(v2);
}

void AppendQuad(
    ModelData& modelData,
    const VertexData& v0,
    const VertexData& v1,
    const VertexData& v2,
    const VertexData& v3)
{
    AppendTriangle(modelData, v0, v1, v2);
    AppendTriangle(modelData, v2, v1, v3);
}

}

void Model::Initialize(ModelCommon* modelCommon,
    const std::string& directoryPath,
    const std::string& filename)
{
    assert(modelCommon);
    modelCommon_ = modelCommon;

    std::filesystem::path requestedPath = filename;
    std::filesystem::path assetDirectoryPath = directoryPath;
    if (!requestedPath.parent_path().empty()) {
        assetDirectoryPath /= requestedPath.parent_path();
    }
    const std::string assetDirectory = assetDirectoryPath.string();
    const std::string assetFilename = requestedPath.filename().string();
    const std::string extension = requestedPath.extension().string();

    // OBJは既存の読み込み、glTF系はAssimpで読み込む
    if (extension == ".gltf" || extension == ".glb") {
        modelData_ = LoadAssimpFile(assetDirectory, assetFilename);
    } else {
        modelData_ = LoadObjFile(assetDirectory, assetFilename);
    }

    // GPUリソース作成
    CreateVertexBuffer();
    CreateMaterial();

    // ★ テクスチャは「読み込むだけ」
    TextureManager::GetInstance()->LoadTexture(
        modelData_.material.textureFilePath
    );
}

void Model::Initialize(ModelCommon* modelCommon, const ModelData& modelData)
{
    assert(modelCommon);
    modelCommon_ = modelCommon;
    modelData_ = modelData;

    CreateVertexBuffer();
    CreateMaterial();

    TextureManager::GetInstance()->LoadTexture(
        modelData_.material.textureFilePath
    );
}


void Model::Draw()
{
    auto dxCommon = modelCommon_->GetDxCommon();
    auto commandList = dxCommon->GetCommandList();
    auto srvManager = modelCommon_->GetSrvManager();

    // VB
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

    // Material CBV
    commandList->SetGraphicsRootConstantBufferView(
        0,
        materialResource_->GetGPUVirtualAddress()
    );

    // Texture SRV（★ index を使う）
    srvManager->SetGraphicsRootDescriptorTable(
        3,
        TextureManager::GetInstance()->GetSrvIndex(
            modelData_.material.textureFilePath
        )
    );

    srvManager->SetGraphicsRootDescriptorTable(
        4,
        TextureManager::GetInstance()->GetSrvIndex(
            modelCommon_->GetEnvironmentTexturePath()
        )
    );

    commandList->DrawInstanced(
        static_cast<UINT>(modelData_.vertices.size()),
        1, 0, 0
    );
}



void Model::CreateVertexBuffer()
{
    auto dxCommon = modelCommon_->GetDxCommon();
    const auto& vertices = modelData_.vertices;

    vertexResource_ = dxCommon->CreateBufferResource(sizeof(VertexData) * vertices.size());

    VertexData* vertexData = nullptr;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
    memcpy(vertexData, vertices.data(), sizeof(VertexData) * vertices.size());
    vertexResource_->Unmap(0, nullptr);

    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * vertices.size());
    vertexBufferView_.StrideInBytes = sizeof(VertexData);
}

void Model::CreateMaterial()
{
    auto dxCommon = modelCommon_->GetDxCommon();

    materialResource_ = dxCommon->CreateBufferResource(sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

    materialData_->color = { 1,1,1,1 };
    materialData_->lightingMode = 2; // Half Lambert
    materialData_->shininess = 64.0f;
    materialData_->environmentCoefficient = 0.2f;
    materialData_->alphaReference = 0.0f;
    materialData_->specularColor = { 1.0f, 1.0f, 1.0f };
    materialData_->uvTransform = MakeIdentity4x4();
}

void Model::SetEnvironmentCoefficient(float coefficient)
{
    materialData_->environmentCoefficient = coefficient;
}

float Model::GetEnvironmentCoefficient() const
{
    return materialData_->environmentCoefficient;
}

void Model::SetColor(const Vector4& color)
{
    materialData_->color = color;
}

void Model::SetAlphaReference(float alphaReference)
{
    materialData_->alphaReference = alphaReference;
}

void Model::SetUVTransform(const Matrix4x4& uvTransform)
{
    materialData_->uvTransform = uvTransform;
}

void Model::SetLightingMode(int32_t lightingMode)
{
    materialData_->lightingMode = lightingMode;
}

MaterialData Model::LoadMaterialTemplate(const std::string& directoryPath, const std::string& filename)
{
    MaterialData materialData;

    std::ifstream file(directoryPath + "/" + filename);
    assert(file.is_open());

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream s(line);
        std::string identifier;
        s >> identifier;

        if (identifier == "map_Kd") {
            std::string textureFilename;
            s >> textureFilename;
            materialData.textureFilePath = directoryPath + "/" + textureFilename;
        }
    }

    return materialData;
}

ModelData Model::LoadObjFile(const std::string& directoryPath, const std::string& filename)
{
    ModelData modelData;

    std::vector<Vector4> positions;
    std::vector<Vector2> texcoords;
    std::vector<Vector3> normals;

    std::ifstream file(directoryPath + "/" + filename);
    assert(file.is_open());

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream s(line);
        std::string id;
        s >> id;

        if (id == "v") {
            Vector4 p{};
            s >> p.x >> p.y >> p.z;
            p.w = 1.0f;

            // 右手→左手（Z反転）
            p.z *= -1.0f;
            positions.push_back(p);

        } else if (id == "vt") {
            Vector2 uv{};
            s >> uv.x >> uv.y;
            uv.y = 1.0f - uv.y; // V反転
            texcoords.push_back(uv);

        } else if (id == "vn") {
            Vector3 n{};
            s >> n.x >> n.y >> n.z;
            n.z *= -1.0f; // 法線Z反転
            normals.push_back(n);

        } else if (id == "f") {
            VertexData tri[3]{};

            for (int i = 0; i < 3; ++i) {
                std::string vStr;
                s >> vStr;
                if (vStr.empty()) continue;

                int idxV = 0, idxT = 0, idxN = 0;
                std::istringstream vs(vStr);
                std::string token;

                if (std::getline(vs, token, '/') && !token.empty()) idxV = std::stoi(token);
                if (std::getline(vs, token, '/') && !token.empty()) idxT = std::stoi(token);
                if (std::getline(vs, token, '/') && !token.empty()) idxN = std::stoi(token);

                Vector4 pos{ 0,0,0,1 };
                if (idxV > 0 && idxV <= (int)positions.size()) pos = positions[idxV - 1];

                Vector2 uv{ 0.0f, 0.0f };
                if (idxT > 0 && idxT <= (int)texcoords.size()) uv = texcoords[idxT - 1];

                Vector3 nor{ 0.0f, 1.0f, 0.0f };
                if (idxN > 0 && idxN <= (int)normals.size()) nor = normals[idxN - 1];

                tri[i] = { pos, uv, nor };
            }

            modelData.vertices.push_back(tri[0]);
            modelData.vertices.push_back(tri[1]);
            modelData.vertices.push_back(tri[2]);

        } else if (id == "mtllib") {
            std::string mtl;
            s >> mtl;
            modelData.material = LoadMaterialTemplate(directoryPath, mtl);
        }
    }

    return modelData;
}

ModelData Model::LoadAssimpFile(
    const std::string& directoryPath,
    const std::string& filename)
{
    ModelData modelData;

    Assimp::Importer importer;
    std::string filePath = directoryPath + "/" + filename;

    const unsigned int flags =
        aiProcess_Triangulate |
        aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices |
        aiProcess_GenNormals;

    const aiScene* scene = importer.ReadFile(filePath.c_str(), flags);
    assert(scene);
    assert(scene->HasMeshes());
    assert(scene->mRootNode);

    modelData.material.textureFilePath =
        GetAssimpTexturePath(scene, directoryPath);
    modelData.rootNode = ReadNode(scene->mRootNode);
    modelData.animation = ReadAnimation(scene);

    AppendAssimpNode(
        scene,
        scene->mRootNode,
        MakeIdentity4x4(),
        modelData);

    return modelData;
}

QuaternionTransform Model::CalculateValue(
    const NodeAnimation& nodeAnimation,
    float time)
{
    QuaternionTransform result{
        { 1.0f, 1.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f }
    };

    auto calculateVector3Value =
        [time](const std::vector<Keyframe<Vector3>>& keyframes, const Vector3& defaultValue) {
            if (keyframes.empty()) {
                return defaultValue;
            }
            if (keyframes.size() == 1 || time <= keyframes.front().time) {
                return keyframes.front().value;
            }

            for (size_t index = 0; index + 1 < keyframes.size(); ++index) {
                const auto& current = keyframes[index];
                const auto& next = keyframes[index + 1];
                if (time <= next.time) {
                    const float duration = next.time - current.time;
                    const float t =
                        duration == 0.0f ? 0.0f : (time - current.time) / duration;
                    return Vector3{
                        current.value.x + (next.value.x - current.value.x) * t,
                        current.value.y + (next.value.y - current.value.y) * t,
                        current.value.z + (next.value.z - current.value.z) * t
                    };
                }
            }

            return keyframes.back().value;
        };

    auto calculateQuaternionValue =
        [time](const std::vector<Keyframe<Quaternion>>& keyframes, const Quaternion& defaultValue) {
            if (keyframes.empty()) {
                return defaultValue;
            }
            if (keyframes.size() == 1 || time <= keyframes.front().time) {
                return NormalizeQuaternion(keyframes.front().value);
            }

            for (size_t index = 0; index + 1 < keyframes.size(); ++index) {
                const auto& current = keyframes[index];
                const auto& next = keyframes[index + 1];
                if (time <= next.time) {
                    const float duration = next.time - current.time;
                    const float t =
                        duration == 0.0f ? 0.0f : (time - current.time) / duration;
                    return NormalizeQuaternion(Slerp(current.value, next.value, t));
                }
            }

            return NormalizeQuaternion(keyframes.back().value);
        };

    result.translate = calculateVector3Value(
        nodeAnimation.translate,
        result.translate
    );
    result.rotate = calculateQuaternionValue(
        nodeAnimation.rotate,
        result.rotate
    );
    result.scale = calculateVector3Value(
        nodeAnimation.scale,
        result.scale
    );

    return result;
}

ModelData Model::CreatePlaneData(
    float width,
    float height,
    const std::string& textureFilePath)
{
    ModelData modelData;
    modelData.material.textureFilePath = textureFilePath;

    const float halfWidth = width * 0.5f;
    const float halfHeight = height * 0.5f;
    const Vector3 normal = { 0.0f, 0.0f, -1.0f };

    modelData.vertices = {
        { { -halfWidth, -halfHeight, 0.0f, 1.0f }, { 0.0f, 1.0f }, normal },
        { { -halfWidth,  halfHeight, 0.0f, 1.0f }, { 0.0f, 0.0f }, normal },
        { {  halfWidth, -halfHeight, 0.0f, 1.0f }, { 1.0f, 1.0f }, normal },
        { {  halfWidth, -halfHeight, 0.0f, 1.0f }, { 1.0f, 1.0f }, normal },
        { { -halfWidth,  halfHeight, 0.0f, 1.0f }, { 0.0f, 0.0f }, normal },
        { {  halfWidth,  halfHeight, 0.0f, 1.0f }, { 1.0f, 0.0f }, normal },
    };

    return modelData;
}

ModelData Model::CreateTriangleData(
    float width,
    float height,
    const std::string& textureFilePath)
{
    assert(width > 0.0f);
    assert(height > 0.0f);

    ModelData modelData;
    modelData.material.textureFilePath = textureFilePath;

    const float halfWidth = width * 0.5f;
    const float halfHeight = height * 0.5f;
    const Vector3 normal = { 0.0f, 0.0f, -1.0f };

    AppendTriangle(
        modelData,
        { { 0.0f, halfHeight, 0.0f, 1.0f }, { 0.5f, 0.0f }, normal },
        { { -halfWidth, -halfHeight, 0.0f, 1.0f }, { 0.0f, 1.0f }, normal },
        { { halfWidth, -halfHeight, 0.0f, 1.0f }, { 1.0f, 1.0f }, normal }
    );

    return modelData;
}

ModelData Model::CreateCircleData(
    uint32_t divideCount,
    float radius,
    const std::string& textureFilePath)
{
    assert(divideCount >= 3);
    assert(radius > 0.0f);

    ModelData modelData;
    modelData.material.textureFilePath = textureFilePath;

    const float radianPerDivide =
        2.0f * std::numbers::pi_v<float> / static_cast<float>(divideCount);
    const Vector3 normal = { 0.0f, 0.0f, -1.0f };
    const VertexData center = {
        { 0.0f, 0.0f, 0.0f, 1.0f },
        { 0.5f, 0.5f },
        normal
    };

    for (uint32_t index = 0; index < divideCount; ++index) {
        const float angle = static_cast<float>(index) * radianPerDivide;
        const float nextAngle =
            static_cast<float>(index + 1) * radianPerDivide;

        const float sinValue = std::sin(angle);
        const float cosValue = std::cos(angle);
        const float sinNext = std::sin(nextAngle);
        const float cosNext = std::cos(nextAngle);

        const VertexData current = {
            { -sinValue * radius, cosValue * radius, 0.0f, 1.0f },
            { 0.5f - sinValue * 0.5f, 0.5f - cosValue * 0.5f },
            normal
        };
        const VertexData next = {
            { -sinNext * radius, cosNext * radius, 0.0f, 1.0f },
            { 0.5f - sinNext * 0.5f, 0.5f - cosNext * 0.5f },
            normal
        };

        AppendTriangle(modelData, center, current, next);
    }

    return modelData;
}

ModelData Model::CreateRingData(
    uint32_t divideCount,
    float outerRadius,
    float innerRadius,
    const std::string& textureFilePath)
{
    assert(divideCount >= 3);
    assert(outerRadius > innerRadius);
    assert(innerRadius >= 0.0f);

    ModelData modelData;
    modelData.material.textureFilePath = textureFilePath;

    const float radianPerDivide =
        2.0f * std::numbers::pi_v<float> / static_cast<float>(divideCount);
    const Vector3 normal = { 0.0f, 0.0f, -1.0f };

    for (uint32_t index = 0; index < divideCount; ++index) {
        const float angle = static_cast<float>(index) * radianPerDivide;
        const float nextAngle =
            static_cast<float>(index + 1) * radianPerDivide;

        const float sinValue = std::sin(angle);
        const float cosValue = std::cos(angle);
        const float sinNext = std::sin(nextAngle);
        const float cosNext = std::cos(nextAngle);

        const float u =
            static_cast<float>(index) / static_cast<float>(divideCount);
        const float uNext =
            static_cast<float>(index + 1) / static_cast<float>(divideCount);

        const VertexData outerCurrent = {
            { -sinValue * outerRadius, cosValue * outerRadius, 0.0f, 1.0f },
            { u, 0.0f },
            normal
        };
        const VertexData outerNext = {
            { -sinNext * outerRadius, cosNext * outerRadius, 0.0f, 1.0f },
            { uNext, 0.0f },
            normal
        };
        const VertexData innerCurrent = {
            { -sinValue * innerRadius, cosValue * innerRadius, 0.0f, 1.0f },
            { u, 1.0f },
            normal
        };
        const VertexData innerNext = {
            { -sinNext * innerRadius, cosNext * innerRadius, 0.0f, 1.0f },
            { uNext, 1.0f },
            normal
        };

        modelData.vertices.push_back(outerCurrent);
        modelData.vertices.push_back(outerNext);
        modelData.vertices.push_back(innerCurrent);
        modelData.vertices.push_back(innerCurrent);
        modelData.vertices.push_back(outerNext);
        modelData.vertices.push_back(innerNext);
    }

    return modelData;
}

ModelData Model::CreateSphereData(
    uint32_t latDivideCount,
    uint32_t lonDivideCount,
    float radius,
    const std::string& textureFilePath)
{
    assert(latDivideCount >= 2);
    assert(lonDivideCount >= 3);
    assert(radius > 0.0f);

    ModelData modelData;
    modelData.material.textureFilePath = textureFilePath;

    const float pi = std::numbers::pi_v<float>;

    for (uint32_t latIndex = 0; latIndex < latDivideCount; ++latIndex) {
        const float v0 =
            static_cast<float>(latIndex) / static_cast<float>(latDivideCount);
        const float v1 =
            static_cast<float>(latIndex + 1) / static_cast<float>(latDivideCount);
        const float theta0 = v0 * pi;
        const float theta1 = v1 * pi;

        for (uint32_t lonIndex = 0; lonIndex < lonDivideCount; ++lonIndex) {
            const float u0 =
                static_cast<float>(lonIndex) / static_cast<float>(lonDivideCount);
            const float u1 =
                static_cast<float>(lonIndex + 1) / static_cast<float>(lonDivideCount);
            const float phi0 = u0 * 2.0f * pi;
            const float phi1 = u1 * 2.0f * pi;

            auto makeVertex = [&](float theta, float phi, float u, float v) {
                const float sinTheta = std::sin(theta);
                const float cosTheta = std::cos(theta);
                const float sinPhi = std::sin(phi);
                const float cosPhi = std::cos(phi);

                const Vector3 normal = {
                    -sinPhi * sinTheta,
                    cosTheta,
                    cosPhi * sinTheta
                };

                return VertexData{
                    {
                        normal.x * radius,
                        normal.y * radius,
                        normal.z * radius,
                        1.0f
                    },
                    { u, v },
                    Normalize(normal)
                };
            };

            const VertexData v00 = makeVertex(theta0, phi0, u0, v0);
            const VertexData v01 = makeVertex(theta0, phi1, u1, v0);
            const VertexData v10 = makeVertex(theta1, phi0, u0, v1);
            const VertexData v11 = makeVertex(theta1, phi1, u1, v1);

            AppendQuad(modelData, v00, v01, v10, v11);
        }
    }

    return modelData;
}

ModelData Model::CreateTorusData(
    uint32_t majorDivideCount,
    uint32_t minorDivideCount,
    float majorRadius,
    float minorRadius,
    const std::string& textureFilePath)
{
    assert(majorDivideCount >= 3);
    assert(minorDivideCount >= 3);
    assert(majorRadius > 0.0f);
    assert(minorRadius > 0.0f);

    ModelData modelData;
    modelData.material.textureFilePath = textureFilePath;

    const float pi = std::numbers::pi_v<float>;

    for (uint32_t majorIndex = 0; majorIndex < majorDivideCount; ++majorIndex) {
        const float u0 =
            static_cast<float>(majorIndex) / static_cast<float>(majorDivideCount);
        const float u1 =
            static_cast<float>(majorIndex + 1) / static_cast<float>(majorDivideCount);
        const float theta0 = u0 * 2.0f * pi;
        const float theta1 = u1 * 2.0f * pi;

        for (uint32_t minorIndex = 0; minorIndex < minorDivideCount; ++minorIndex) {
            const float v0 =
                static_cast<float>(minorIndex) / static_cast<float>(minorDivideCount);
            const float v1 =
                static_cast<float>(minorIndex + 1) / static_cast<float>(minorDivideCount);
            const float phi0 = v0 * 2.0f * pi;
            const float phi1 = v1 * 2.0f * pi;

            auto makeVertex = [&](float theta, float phi, float u, float v) {
                const float sinTheta = std::sin(theta);
                const float cosTheta = std::cos(theta);
                const float sinPhi = std::sin(phi);
                const float cosPhi = std::cos(phi);

                const float ringRadius = majorRadius + minorRadius * cosPhi;
                const Vector3 normal = Normalize({
                    -cosPhi * sinTheta,
                    sinPhi,
                    cosPhi * cosTheta
                });

                return VertexData{
                    {
                        -sinTheta * ringRadius,
                        sinPhi * minorRadius,
                        cosTheta * ringRadius,
                        1.0f
                    },
                    { u, v },
                    normal
                };
            };

            const VertexData v00 = makeVertex(theta0, phi0, u0, v0);
            const VertexData v01 = makeVertex(theta1, phi0, u1, v0);
            const VertexData v10 = makeVertex(theta0, phi1, u0, v1);
            const VertexData v11 = makeVertex(theta1, phi1, u1, v1);

            AppendQuad(modelData, v00, v01, v10, v11);
        }
    }

    return modelData;
}

ModelData Model::CreateCylinderData(
    uint32_t divideCount,
    float topRadius,
    float bottomRadius,
    float height,
    const std::string& textureFilePath)
{
    assert(divideCount >= 3);
    assert(topRadius >= 0.0f);
    assert(bottomRadius >= 0.0f);
    assert(height > 0.0f);

    ModelData modelData;
    modelData.material.textureFilePath = textureFilePath;

    const float radianPerDivide =
        2.0f * std::numbers::pi_v<float> / static_cast<float>(divideCount);

    for (uint32_t index = 0; index < divideCount; ++index) {
        const float angle = static_cast<float>(index) * radianPerDivide;
        const float nextAngle =
            static_cast<float>(index + 1) * radianPerDivide;

        const float sinValue = std::sin(angle);
        const float cosValue = std::cos(angle);
        const float sinNext = std::sin(nextAngle);
        const float cosNext = std::cos(nextAngle);

        const float u =
            static_cast<float>(index) / static_cast<float>(divideCount);
        const float uNext =
            static_cast<float>(index + 1) / static_cast<float>(divideCount);

        const Vector3 normalCurrent =
            Normalize({ -sinValue, 0.0f, cosValue });
        const Vector3 normalNext =
            Normalize({ -sinNext, 0.0f, cosNext });

        const VertexData topCurrent = {
            { -sinValue * topRadius, height, cosValue * topRadius, 1.0f },
            { u, 0.0f },
            normalCurrent
        };
        const VertexData topNext = {
            { -sinNext * topRadius, height, cosNext * topRadius, 1.0f },
            { uNext, 0.0f },
            normalNext
        };
        const VertexData bottomCurrent = {
            { -sinValue * bottomRadius, 0.0f, cosValue * bottomRadius, 1.0f },
            { u, 1.0f },
            normalCurrent
        };
        const VertexData bottomNext = {
            { -sinNext * bottomRadius, 0.0f, cosNext * bottomRadius, 1.0f },
            { uNext, 1.0f },
            normalNext
        };

        modelData.vertices.push_back(topCurrent);
        modelData.vertices.push_back(topNext);
        modelData.vertices.push_back(bottomCurrent);
        modelData.vertices.push_back(bottomCurrent);
        modelData.vertices.push_back(topNext);
        modelData.vertices.push_back(bottomNext);
    }

    return modelData;
}

ModelData Model::CreateConeData(
    uint32_t divideCount,
    float radius,
    float height,
    const std::string& textureFilePath)
{
    assert(divideCount >= 3);
    assert(radius > 0.0f);
    assert(height > 0.0f);

    ModelData modelData;
    modelData.material.textureFilePath = textureFilePath;

    const float radianPerDivide =
        2.0f * std::numbers::pi_v<float> / static_cast<float>(divideCount);
    const VertexData apex = {
        { 0.0f, height, 0.0f, 1.0f },
        { 0.5f, 0.0f },
        { 0.0f, 1.0f, 0.0f }
    };

    for (uint32_t index = 0; index < divideCount; ++index) {
        const float angle = static_cast<float>(index) * radianPerDivide;
        const float nextAngle =
            static_cast<float>(index + 1) * radianPerDivide;

        const float sinValue = std::sin(angle);
        const float cosValue = std::cos(angle);
        const float sinNext = std::sin(nextAngle);
        const float cosNext = std::cos(nextAngle);

        const float u =
            static_cast<float>(index) / static_cast<float>(divideCount);
        const float uNext =
            static_cast<float>(index + 1) / static_cast<float>(divideCount);

        const Vector3 normalCurrent =
            Normalize({ -sinValue * height, radius, cosValue * height });
        const Vector3 normalNext =
            Normalize({ -sinNext * height, radius, cosNext * height });

        const VertexData current = {
            { -sinValue * radius, 0.0f, cosValue * radius, 1.0f },
            { u, 1.0f },
            normalCurrent
        };
        const VertexData next = {
            { -sinNext * radius, 0.0f, cosNext * radius, 1.0f },
            { uNext, 1.0f },
            normalNext
        };

        AppendTriangle(modelData, apex, current, next);
    }

    return modelData;
}

ModelData Model::CreateBoxData(
    float width,
    float height,
    float depth,
    const std::string& textureFilePath)
{
    assert(width > 0.0f);
    assert(height > 0.0f);
    assert(depth > 0.0f);

    ModelData modelData;
    modelData.material.textureFilePath = textureFilePath;

    const float halfWidth = width * 0.5f;
    const float halfHeight = height * 0.5f;
    const float halfDepth = depth * 0.5f;

    const Vector4 leftTopFront = { -halfWidth, halfHeight, -halfDepth, 1.0f };
    const Vector4 rightTopFront = { halfWidth, halfHeight, -halfDepth, 1.0f };
    const Vector4 leftBottomFront = { -halfWidth, -halfHeight, -halfDepth, 1.0f };
    const Vector4 rightBottomFront = { halfWidth, -halfHeight, -halfDepth, 1.0f };
    const Vector4 leftTopBack = { -halfWidth, halfHeight, halfDepth, 1.0f };
    const Vector4 rightTopBack = { halfWidth, halfHeight, halfDepth, 1.0f };
    const Vector4 leftBottomBack = { -halfWidth, -halfHeight, halfDepth, 1.0f };
    const Vector4 rightBottomBack = { halfWidth, -halfHeight, halfDepth, 1.0f };

    auto makeVertex = [](const Vector4& position, const Vector2& uv, const Vector3& normal) {
        return VertexData{ position, uv, normal };
    };

    AppendQuad(
        modelData,
        makeVertex(leftTopFront, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }),
        makeVertex(rightTopFront, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }),
        makeVertex(leftBottomFront, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }),
        makeVertex(rightBottomFront, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f })
    );
    AppendQuad(
        modelData,
        makeVertex(rightTopBack, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }),
        makeVertex(leftTopBack, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }),
        makeVertex(rightBottomBack, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }),
        makeVertex(leftBottomBack, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f })
    );
    AppendQuad(
        modelData,
        makeVertex(leftTopBack, { 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }),
        makeVertex(leftTopFront, { 1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }),
        makeVertex(leftBottomBack, { 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f }),
        makeVertex(leftBottomFront, { 1.0f, 1.0f }, { -1.0f, 0.0f, 0.0f })
    );
    AppendQuad(
        modelData,
        makeVertex(rightTopFront, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }),
        makeVertex(rightTopBack, { 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }),
        makeVertex(rightBottomFront, { 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }),
        makeVertex(rightBottomBack, { 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f })
    );
    AppendQuad(
        modelData,
        makeVertex(leftTopBack, { 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }),
        makeVertex(rightTopBack, { 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }),
        makeVertex(leftTopFront, { 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f }),
        makeVertex(rightTopFront, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f })
    );
    AppendQuad(
        modelData,
        makeVertex(leftBottomFront, { 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }),
        makeVertex(rightBottomFront, { 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }),
        makeVertex(leftBottomBack, { 0.0f, 1.0f }, { 0.0f, -1.0f, 0.0f }),
        makeVertex(rightBottomBack, { 1.0f, 1.0f }, { 0.0f, -1.0f, 0.0f })
    );

    return modelData;
}
