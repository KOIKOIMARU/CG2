#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>

#include <d3d12.h>
#include <wrl/client.h>

#include "engine/base/Math.h"
#include "engine/base/DirectXCommon.h"
#include "engine/3d/TextureManager.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/3d/Object3d.h"


void Object3d::Initialize(Object3dCommon* object3dCommon) {
    assert(object3dCommon);
    object3dCommon_ = object3dCommon;

    // ★ OBJ読み込み（資料外だが既存）
    modelData_ = LoadObjFile("resources", "plane.obj");

    // ★ 資料のメイン処理
    CreateVertexBuffer();
    CreateMaterial();
    CreateTransformationMatrix();
    CreateDirectionalLight();

    // === Transform 初期化（資料該当） ===
    transform_ = {
        {1.0f, 1.0f, 1.0f}, // scale
        {0.0f, 0.0f, 0.0f}, // rotate
        {0.0f, 0.0f, 0.0f}  // translate
    };

    cameraTransform_ = {
        {1.0f, 1.0f, 1.0f}, // scale
        {0.3f, 0.0f, 0.0f}, // rotate
        {0.0f, 4.0f, -10.0f} // translate
    };

    // .obj が参照しているテクスチャを読み込む
    TextureManager::GetInstance()->LoadTexture(
        modelData_.material.textureFilePath);

    // 読み込んだテクスチャ番号を取得
    modelData_.material.textureIndex =
        TextureManager::GetInstance()->GetTextureIndexByFilePath(
            modelData_.material.textureFilePath);

}

void Object3d::Update() {
    // World行列
    Matrix4x4 worldMatrix = MakeAffineMatrix(
        transform_.scale,
        transform_.rotate,
        transform_.translate
    );

    // Camera行列 → View行列
    Matrix4x4 cameraMatrix = MakeAffineMatrix(
        cameraTransform_.scale,
        cameraTransform_.rotate,
        cameraTransform_.translate
    );
    Matrix4x4 viewMatrix = Inverse(cameraMatrix);

    // Projection行列
    Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(
        0.45f,
        float(WinApp::kClientWidth) / float(WinApp::kClientHeight),
        0.1f,
        100.0f
    );

    Matrix4x4 viewProjectionMatrix =
        Multiply(viewMatrix, projectionMatrix);

    // CBへ書き込み
    transformationMatrixData_->WVP =
        Transpose(Multiply(worldMatrix, viewProjectionMatrix));
    transformationMatrixData_->World =
        Transpose(worldMatrix);
}

void Object3d::Draw() {
    auto commandList = object3dCommon_->GetDxCommon()->GetCommandList();

    // VertexBuffer
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

    // Material
    commandList->SetGraphicsRootConstantBufferView(
        0, materialResource_->GetGPUVirtualAddress());

    // Transform
    commandList->SetGraphicsRootConstantBufferView(
        1, transformationMatrixResource_->GetGPUVirtualAddress());

    // Texture
    commandList->SetGraphicsRootDescriptorTable(
        2,
        TextureManager::GetInstance()->GetSrvHandleGPU(
            modelData_.material.textureIndex
        )
    );

    // DirectionalLight
    commandList->SetGraphicsRootConstantBufferView(
        3, directionalLightResource_->GetGPUVirtualAddress());

    // Draw
    commandList->DrawInstanced(
        static_cast<UINT>(modelData_.vertices.size()),
        1, 0, 0
    );
}

MaterialData Object3d::LoadMaterialTemplate(const std::string& directoryPath, const std::string& filename)
{
    MaterialData materialData;
    std::string line; // ファイルから読んだ1行を格納するもの
    std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
    assert(file.is_open()); // ファイルが開けなかったらエラー
    while (std::getline(file, line)) {
        std::string identifier;
        std::istringstream s(line);
        s >> identifier;

        // identifierに応じた処理
        if (identifier == "map_Kd") {
            std::string textureFilename;
            s >> textureFilename;
            // 連結してファイルパスにする
            materialData.textureFilePath = directoryPath + "/" + textureFilename;
        }
    }
    return materialData;
}

ModelData Object3d::LoadObjFile(const std::string& directoryPath, const std::string& filename)
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

            // 右手系 → 左手系（Z反転）
            p.z *= -1.0f;
            positions.push_back(p);

        } else if (id == "vt") {
            Vector2 uv{};
            s >> uv.x >> uv.y;
            // DirectX 用に V 反転
            uv.y = 1.0f - uv.y;
            texcoords.push_back(uv);

        } else if (id == "vn") {
            Vector3 n{};
            s >> n.x >> n.y >> n.z;
            // 法線も Z 反転
            n.z *= -1.0f;
            normals.push_back(n);

        } else if (id == "f") {
            // f v/t/n  or v//n or v/t のどれでもOKにする
            VertexData tri[3]{};

            for (int i = 0; i < 3; ++i) {
                std::string vStr;
                s >> vStr;
                if (vStr.empty()) continue;

                int idxV = 0, idxT = 0, idxN = 0;

                std::istringstream vs(vStr);
                std::string token;

                // v
                if (std::getline(vs, token, '/') && !token.empty()) {
                    idxV = std::stoi(token);
                }
                // t（無い場合は空文字）
                if (std::getline(vs, token, '/') && !token.empty()) {
                    idxT = std::stoi(token);
                }
                // n（無い場合は空文字）
                if (std::getline(vs, token, '/') && !token.empty()) {
                    idxN = std::stoi(token);
                }

                // 安全に参照
                Vector4 pos{ 0,0,0,1 };
                if (idxV > 0 && idxV <= (int)positions.size()) {
                    pos = positions[idxV - 1];
                }

                Vector2 uv{ 0.0f, 0.0f };
                if (idxT > 0 && idxT <= (int)texcoords.size()) {
                    uv = texcoords[idxT - 1];
                }

                Vector3 nor{ 0.0f, 1.0f, 0.0f };
                if (idxN > 0 && idxN <= (int)normals.size()) {
                    nor = normals[idxN - 1];
                }

                tri[i] = { pos, uv, nor };
            }

            // 左手系なので順番そのままでOK（OBJは通常CCW）
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



void Object3d::CreateVertexBuffer() {
    auto dxCommon = object3dCommon_->GetDxCommon();
    ID3D12Device* device = dxCommon->GetDevice();

    const auto& vertices = modelData_.vertices;

    // VertexResource を作る
    vertexResource_ =
        dxCommon->CreateBufferResource(sizeof(VertexData) * vertices.size());

    // Map
    VertexData* vertexData = nullptr;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
    memcpy(vertexData, vertices.data(), sizeof(VertexData) * vertices.size());
    vertexResource_->Unmap(0, nullptr);

    // VertexBufferView を作る
    vertexBufferView_.BufferLocation =
        vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes =
        UINT(sizeof(VertexData) * vertices.size());
    vertexBufferView_.StrideInBytes = sizeof(VertexData);
}

void Object3d::CreateMaterial() {
    auto dxCommon = object3dCommon_->GetDxCommon();

    materialResource_ =
        dxCommon->CreateBufferResource(sizeof(Material));

    materialResource_->Map(
        0, nullptr, reinterpret_cast<void**>(&materialData_));

    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_->lightingMode = 1; // Lambert
    materialData_->uvTransform = MakeIdentity4x4();
}

void Object3d::CreateTransformationMatrix() {
    auto dxCommon = object3dCommon_->GetDxCommon();

    // 座標変換行列用リソースを作成
    transformationMatrixResource_ =
        dxCommon->CreateBufferResource(sizeof(TransformationMatrix));

    // Map してポインタ取得
    transformationMatrixResource_->Map(
        0, nullptr,
        reinterpret_cast<void**>(&transformationMatrixData_));

    // 単位行列で初期化
    transformationMatrixData_->WVP = MakeIdentity4x4();
    transformationMatrixData_->World = MakeIdentity4x4();
}

void Object3d::CreateDirectionalLight() {
    auto dxCommon = object3dCommon_->GetDxCommon();

    // 平行光源用 ConstantBuffer を作成
    directionalLightResource_ =
        dxCommon->CreateBufferResource(sizeof(DirectionalLight));

    // Map
    directionalLightResource_->Map(
        0, nullptr,
        reinterpret_cast<void**>(&directionalLightData_));

    // 初期化（資料準拠）
    directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    directionalLightData_->direction = { 0.0f, -1.0f, 0.0f };
    directionalLightData_->intensity = 1.0f;
}
