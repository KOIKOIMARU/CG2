#include "engine/3d/Model.h"

#include <fstream>
#include <sstream>
#include <cassert>

#include "engine/3d/ModelCommon.h"
#include "engine/base/DirectXCommon.h"
#include "engine/3d/TextureManager.h"

void Model::Initialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename)
{
    assert(modelCommon);
    modelCommon_ = modelCommon;

    // OBJ 読み込み
    modelData_ = LoadObjFile(directoryPath, filename);

    // GPUリソース作成
    CreateVertexBuffer();
    CreateMaterial();

    // テクスチャ読み込み → Index 取得
    TextureManager::GetInstance()->LoadTexture(modelData_.material.textureFilePath);
    modelData_.material.textureIndex =
        TextureManager::GetInstance()->GetTextureIndexByFilePath(modelData_.material.textureFilePath);
}

void Model::Draw()
{
    auto commandList = modelCommon_->GetDxCommon()->GetCommandList();

    // VB
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

    // Material CBV
    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

    // Texture SRV
    commandList->SetGraphicsRootDescriptorTable(
        2,
        TextureManager::GetInstance()->GetSrvHandleGPU(modelData_.material.textureIndex)
    );

    // Draw
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
    materialData_->lightingMode = 1; // Lambert
    materialData_->uvTransform = MakeIdentity4x4();
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
