#include <vector>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <d3d12.h>
#include <wrl/client.h>

#include "engine/base/Math.h"
#include "engine/base/DirectXCommon.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Model.h"
#include "engine/3d/ModelManager.h"
#include "engine/3d/Camera.h"
#include "engine/base/SrvManager.h"



void Object3d::Initialize(Object3dCommon* object3dCommon)
{
    assert(object3dCommon);
    object3dCommon_ = object3dCommon;

    camera_ = object3dCommon_->GetDefaultCamera();

    CreateTransformationMatrix();
    CreateDirectionalLight();
    CreateCameraResource();
    CreatePointLight();
    CreateSpotLight();
    CreateSkinningPalette();
    CreateComputeSkinningPipeline();

    transform_ = {
        {1.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f}
    };

}


void Object3d::Update() {
    // ワールド行列
    Matrix4x4 worldMatrix;
    if (useQuaternionRotate_) {
        worldMatrix = MakeAffineMatrix(
            transform_.scale,
            quaternionRotate_,
            transform_.translate
        );
    } else {
        worldMatrix = MakeAffineMatrix(
            transform_.scale,
            transform_.rotate,
            transform_.translate
        );
    }
    worldMatrix_ = worldMatrix;

    Matrix4x4 worldViewProjectionMatrix;

    if (camera_) {
        const Matrix4x4& viewProjection =
            camera_->GetViewProjectionMatrix();
        worldViewProjectionMatrix =
            Multiply(worldMatrix, viewProjection);
    } else {
        // カメラが無くても一応描画可能
        worldViewProjectionMatrix = worldMatrix;
    }

    transformationMatrixData_->WVP =
        Transpose(worldViewProjectionMatrix);
    transformationMatrixData_->World =
        Transpose(worldMatrix);
    transformationMatrixData_->WorldInverseTranspose =
        Transpose(Inverse(worldMatrix));

    if (camera_) {
        cameraData_->worldPosition = camera_->GetTranslate();
    }
}

void Object3d::UpdateAnimation(float deltaTime)
{
    if (!model_ || !hasSkeleton_) {
        if (skinningPaletteData_) {
            skinningPaletteData_->enableSkinning = 0;
        }
        return;
    }

    const Animation& animation = model_->GetAnimation();
    if (animation.duration > 0.0f && model_->HasAnimation()) {
        animationTime_ += deltaTime * animation.ticksPerSecond;
        while (animationTime_ > animation.duration) {
            animationTime_ -= animation.duration;
        }
        Model::ApplyAnimation(skeleton_, animation, animationTime_);
    } else {
        for (Joint& joint : skeleton_.joints) {
            joint.transform = joint.bindPoseTransform;
        }
    }

    Model::UpdateSkeleton(skeleton_);
    UpdateSkinningPalette();
}



void Object3d::Draw()
{
    auto commandList = object3dCommon_->GetDxCommon()->GetCommandList();

    // Transform
    commandList->SetGraphicsRootConstantBufferView(
        1, transformationMatrixResource_->GetGPUVirtualAddress());

    // Light
    commandList->SetGraphicsRootConstantBufferView(
        2, cameraResource_->GetGPUVirtualAddress());

    commandList->SetGraphicsRootConstantBufferView(
        5, directionalLightResource_->GetGPUVirtualAddress());

    commandList->SetGraphicsRootConstantBufferView(
        6, pointLightResource_->GetGPUVirtualAddress());

    commandList->SetGraphicsRootConstantBufferView(
        7, spotLightResource_->GetGPUVirtualAddress());

    commandList->SetGraphicsRootConstantBufferView(
        8, skinningPaletteResource_->GetGPUVirtualAddress());

    // Model 描画
    if (enableComputeSkinning_) {
        DispatchComputeSkinning();
        object3dCommon_->CommonDrawSetting();
    }

    commandList->SetGraphicsRootConstantBufferView(
        1, transformationMatrixResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(
        2, cameraResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(
        5, directionalLightResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(
        6, pointLightResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(
        7, spotLightResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(
        8, skinningPaletteResource_->GetGPUVirtualAddress());

    if (model_) {
        if (enableComputeSkinning_) {
            model_->Draw(&computeOutputVertexBufferView_);
        } else {
            model_->Draw();
        }
    }
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
    transformationMatrixData_->WorldInverseTranspose = MakeIdentity4x4();
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

void Object3d::CreateCameraResource() {
    auto dxCommon = object3dCommon_->GetDxCommon();

    cameraResource_ =
        dxCommon->CreateBufferResource(sizeof(CameraForGPU));

    cameraResource_->Map(
        0, nullptr,
        reinterpret_cast<void**>(&cameraData_));

    cameraData_->worldPosition = { 0.0f, 0.0f, -5.0f };
    cameraData_->padding = 0.0f;
}

void Object3d::CreatePointLight() {
    auto dxCommon = object3dCommon_->GetDxCommon();

    pointLightResource_ =
        dxCommon->CreateBufferResource(sizeof(PointLight));

    pointLightResource_->Map(
        0, nullptr,
        reinterpret_cast<void**>(&pointLightData_));

    pointLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    pointLightData_->position = { 0.0f, 2.0f, 0.0f };
    pointLightData_->intensity = 1.0f;
    pointLightData_->radius = 6.0f;
    pointLightData_->decay = 2.0f;
}

void Object3d::CreateSpotLight() {
    auto dxCommon = object3dCommon_->GetDxCommon();

    spotLightResource_ =
        dxCommon->CreateBufferResource(sizeof(SpotLight));

    spotLightResource_->Map(
        0, nullptr,
        reinterpret_cast<void**>(&spotLightData_));

    spotLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    spotLightData_->position = { 2.0f, 1.25f, 0.0f };
    spotLightData_->direction = Normalize({ -1.0f, 1.0f, 0.0f });
    spotLightData_->intensity = 4.0f;
    spotLightData_->distance = 7.0f;
    spotLightData_->decay = 2.0f;
    spotLightData_->cosAngle = std::cos(3.14159265f / 3.0f);
    spotLightData_->cosFalloffStart = std::cos(3.14159265f / 6.0f);
}

void Object3d::CreateSkinningPalette()
{
    auto dxCommon = object3dCommon_->GetDxCommon();

    skinningPaletteResource_ =
        dxCommon->CreateBufferResource(sizeof(SkinningPaletteForGPU));

    skinningPaletteResource_->Map(
        0, nullptr,
        reinterpret_cast<void**>(&skinningPaletteData_));

    skinningPaletteData_->enableSkinning = 0;
    for (uint32_t jointIndex = 0; jointIndex < kNumMaxSkeletonJoints; ++jointIndex) {
        skinningPaletteData_->palette[jointIndex].skeletonSpaceMatrix =
            MakeIdentity4x4();
        skinningPaletteData_->palette[jointIndex].skeletonSpaceInverseTransposeMatrix =
            MakeIdentity4x4();
    }
}

void Object3d::CreateComputeSkinningPipeline()
{
    auto* device = object3dCommon_->GetDxCommon()->GetDevice();
    HRESULT hr = S_OK;

    D3D12_DESCRIPTOR_RANGE descriptorRanges[4]{};
    descriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRanges[0].NumDescriptors = 1;
    descriptorRanges[0].BaseShaderRegister = 0;
    descriptorRanges[0].OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    descriptorRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRanges[1].NumDescriptors = 1;
    descriptorRanges[1].BaseShaderRegister = 1;
    descriptorRanges[1].OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    descriptorRanges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRanges[2].NumDescriptors = 1;
    descriptorRanges[2].BaseShaderRegister = 2;
    descriptorRanges[2].OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    descriptorRanges[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    descriptorRanges[3].NumDescriptors = 1;
    descriptorRanges[3].BaseShaderRegister = 0;
    descriptorRanges[3].OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[5]{};
    for (uint32_t parameterIndex = 0; parameterIndex < 4; ++parameterIndex) {
        rootParameters[parameterIndex].ParameterType =
            D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[parameterIndex].ShaderVisibility =
            D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[parameterIndex].DescriptorTable.pDescriptorRanges =
            &descriptorRanges[parameterIndex];
        rootParameters[parameterIndex].DescriptorTable.NumDescriptorRanges = 1;
    }

    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[4].Descriptor.ShaderRegister = 0;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumParameters = _countof(rootParameters);

    ID3DBlob* signatureBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    hr = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob
    );
    if (FAILED(hr)) {
        assert(false);
    }

    hr = device->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&computeRootSignature_)
    );
    assert(SUCCEEDED(hr));

    auto computeShaderBlob =
        object3dCommon_->GetDxCommon()->CompileShader(
            L"shaders/Skinning.CS.hlsl",
            L"cs_6_0"
        );

    D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDesc{};
    computePipelineStateDesc.pRootSignature = computeRootSignature_.Get();
    computePipelineStateDesc.CS = {
        computeShaderBlob->GetBufferPointer(),
        computeShaderBlob->GetBufferSize()
    };

    hr = device->CreateComputePipelineState(
        &computePipelineStateDesc,
        IID_PPV_ARGS(&computePipelineState_)
    );
    assert(SUCCEEDED(hr));
}

void Object3d::ReleaseComputeSkinningResources()
{
    if (!object3dCommon_) {
        return;
    }

    SrvManager* srvManager = object3dCommon_->GetSrvManager();
    if (srvManager) {
        if (computePaletteSrvIndex_ != UINT32_MAX) {
            srvManager->Free(computePaletteSrvIndex_);
        }
        if (computeInputVertexSrvIndex_ != UINT32_MAX) {
            srvManager->Free(computeInputVertexSrvIndex_);
        }
        if (computeInfluenceSrvIndex_ != UINT32_MAX) {
            srvManager->Free(computeInfluenceSrvIndex_);
        }
        if (computeOutputVertexUavIndex_ != UINT32_MAX) {
            srvManager->Free(computeOutputVertexUavIndex_);
        }
    }

    computePaletteSrvIndex_ = UINT32_MAX;
    computeInputVertexSrvIndex_ = UINT32_MAX;
    computeInfluenceSrvIndex_ = UINT32_MAX;
    computeOutputVertexUavIndex_ = UINT32_MAX;
    computeInputVertexResource_.Reset();
    computeInfluenceResource_.Reset();
    computeMatrixPaletteResource_.Reset();
    computeOutputVertexResource_.Reset();
    computeSkinningInfoResource_.Reset();
    computeMatrixPaletteData_ = nullptr;
    computeSkinningInfoData_ = nullptr;
    computeOutputVertexBufferView_ = {};
    computeOutputVertexState_ = D3D12_RESOURCE_STATE_COMMON;
}

void Object3d::InitializeComputeSkinningResources()
{
    if (!model_ || !hasSkeleton_) {
        return;
    }

    auto* dxCommon = object3dCommon_->GetDxCommon();
    auto* srvManager = object3dCommon_->GetSrvManager();
    const auto& vertices = model_->GetVertices();
    if (vertices.empty()) {
        return;
    }

    std::vector<SkinningVertexForCompute> inputVertices(vertices.size());
    std::vector<VertexInfluenceForCompute> influences(vertices.size());
    for (size_t vertexIndex = 0; vertexIndex < vertices.size(); ++vertexIndex) {
        inputVertices[vertexIndex].position = vertices[vertexIndex].position;
        inputVertices[vertexIndex].texcoord = vertices[vertexIndex].texcoord;
        inputVertices[vertexIndex].normal = vertices[vertexIndex].normal;
        influences[vertexIndex].weight = vertices[vertexIndex].weight;
        for (uint32_t influenceIndex = 0; influenceIndex < kNumMaxInfluence; ++influenceIndex) {
            influences[vertexIndex].jointIndices[influenceIndex] =
                static_cast<int32_t>(vertices[vertexIndex].jointIndices[influenceIndex]);
        }
    }

    computeInputVertexResource_ = dxCommon->CreateBufferResource(
        sizeof(SkinningVertexForCompute) * inputVertices.size());
    SkinningVertexForCompute* mappedInputVertices = nullptr;
    computeInputVertexResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&mappedInputVertices)
    );
    memcpy(
        mappedInputVertices,
        inputVertices.data(),
        sizeof(SkinningVertexForCompute) * inputVertices.size()
    );
    computeInputVertexResource_->Unmap(0, nullptr);

    computeInfluenceResource_ = dxCommon->CreateBufferResource(
        sizeof(VertexInfluenceForCompute) * influences.size());
    VertexInfluenceForCompute* mappedInfluences = nullptr;
    computeInfluenceResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&mappedInfluences)
    );
    memcpy(
        mappedInfluences,
        influences.data(),
        sizeof(VertexInfluenceForCompute) * influences.size()
    );
    computeInfluenceResource_->Unmap(0, nullptr);

    computeMatrixPaletteResource_ = dxCommon->CreateBufferResource(
        sizeof(WellForGPU) * kNumMaxSkeletonJoints);
    computeMatrixPaletteResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&computeMatrixPaletteData_)
    );

    computeSkinningInfoResource_ = dxCommon->CreateBufferResource(
        sizeof(SkinningInformationForCompute));
    computeSkinningInfoResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&computeSkinningInfoData_)
    );
    computeSkinningInfoData_->numVertices = static_cast<uint32_t>(vertices.size());

    computeOutputVertexResource_ = dxCommon->CreateBufferResource(
        sizeof(VertexData) * vertices.size(),
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
    );
    computeOutputVertexBufferView_.BufferLocation =
        computeOutputVertexResource_->GetGPUVirtualAddress();
    computeOutputVertexBufferView_.SizeInBytes =
        static_cast<UINT>(sizeof(VertexData) * vertices.size());
    computeOutputVertexBufferView_.StrideInBytes = sizeof(VertexData);
    computeOutputVertexState_ = D3D12_RESOURCE_STATE_COMMON;

    computePaletteSrvIndex_ = srvManager->Allocate();
    computeInputVertexSrvIndex_ = srvManager->Allocate();
    computeInfluenceSrvIndex_ = srvManager->Allocate();
    computeOutputVertexUavIndex_ = srvManager->Allocate();

    srvManager->CreateSRVforStructuredBuffer(
        computePaletteSrvIndex_,
        computeMatrixPaletteResource_.Get(),
        kNumMaxSkeletonJoints,
        sizeof(WellForGPU)
    );
    srvManager->CreateSRVforStructuredBuffer(
        computeInputVertexSrvIndex_,
        computeInputVertexResource_.Get(),
        static_cast<UINT>(inputVertices.size()),
        sizeof(SkinningVertexForCompute)
    );
    srvManager->CreateSRVforStructuredBuffer(
        computeInfluenceSrvIndex_,
        computeInfluenceResource_.Get(),
        static_cast<UINT>(influences.size()),
        sizeof(VertexInfluenceForCompute)
    );
    srvManager->CreateUAVforStructuredBuffer(
        computeOutputVertexUavIndex_,
        computeOutputVertexResource_.Get(),
        static_cast<UINT>(vertices.size()),
        sizeof(VertexData)
    );

    enableComputeSkinning_ = true;
    UpdateSkinningPalette();
}

void Object3d::DispatchComputeSkinning()
{
    if (!enableComputeSkinning_ || !model_) {
        return;
    }

    auto* dxCommon = object3dCommon_->GetDxCommon();
    auto* srvManager = object3dCommon_->GetSrvManager();
    auto* commandList = dxCommon->GetCommandList();
    const uint32_t vertexCount = static_cast<uint32_t>(model_->GetVertexCount());
    if (vertexCount == 0) {
        return;
    }

    srvManager->PreDraw();

    if (computeOutputVertexState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = computeOutputVertexResource_.Get();
        barrier.Transition.StateBefore = computeOutputVertexState_;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &barrier);
        computeOutputVertexState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

    commandList->SetComputeRootSignature(computeRootSignature_.Get());
    commandList->SetPipelineState(computePipelineState_.Get());
    srvManager->SetComputeRootDescriptorTable(0, computePaletteSrvIndex_);
    srvManager->SetComputeRootDescriptorTable(1, computeInputVertexSrvIndex_);
    srvManager->SetComputeRootDescriptorTable(2, computeInfluenceSrvIndex_);
    srvManager->SetComputeRootDescriptorTable(3, computeOutputVertexUavIndex_);
    commandList->SetComputeRootConstantBufferView(
        4,
        computeSkinningInfoResource_->GetGPUVirtualAddress()
    );
    commandList->Dispatch((vertexCount + 1023u) / 1024u, 1, 1);

    D3D12_RESOURCE_BARRIER uavBarrier{};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = computeOutputVertexResource_.Get();
    commandList->ResourceBarrier(1, &uavBarrier);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = computeOutputVertexResource_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier.Transition.StateAfter =
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);
    computeOutputVertexState_ = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
}

void Object3d::InitializeSkinning()
{
    hasSkeleton_ = false;
    enableComputeSkinning_ = false;
    inverseBindPoseMatrices_.clear();
    animationTime_ = 0.0f;
    ReleaseComputeSkinningResources();

    if (!model_ || !model_->HasSkinCluster()) {
        if (skinningPaletteData_) {
            skinningPaletteData_->enableSkinning = 0;
        }
        return;
    }

    skeleton_ = Model::CreateSkeleton(model_->GetRootNode());
    inverseBindPoseMatrices_.assign(
        skeleton_.joints.size(),
        MakeIdentity4x4()
    );

    const SkinClusterData& skinClusterData = model_->GetSkinClusterData();
    for (const auto& [jointName, jointWeightData] : skinClusterData.jointWeights) {
        auto jointIt = skeleton_.jointMap.find(jointName);
        if (jointIt == skeleton_.jointMap.end()) {
            continue;
        }

        inverseBindPoseMatrices_[jointIt->second] =
            jointWeightData.inverseBindPoseMatrix;
    }

    Model::UpdateSkeleton(skeleton_);
    UpdateSkinningPalette();
    hasSkeleton_ = true;
    InitializeComputeSkinningResources();
}

void Object3d::UpdateSkinningPalette()
{
    if (!skinningPaletteData_) {
        return;
    }

    if (!hasSkeleton_) {
        skinningPaletteData_->enableSkinning = 0;
        return;
    }

    skinningPaletteData_->enableSkinning = enableComputeSkinning_ ? 0 : 1;
    const size_t jointCount =
        std::min<size_t>(skeleton_.joints.size(), kNumMaxSkeletonJoints);

    for (size_t jointIndex = 0; jointIndex < jointCount; ++jointIndex) {
        const Matrix4x4 skinningMatrix = Multiply(
            inverseBindPoseMatrices_[jointIndex],
            skeleton_.joints[jointIndex].skeletonSpaceMatrix
        );

        skinningPaletteData_->palette[jointIndex].skeletonSpaceMatrix =
            Transpose(skinningMatrix);
        skinningPaletteData_->palette[jointIndex].skeletonSpaceInverseTransposeMatrix =
            Transpose(Inverse(skinningMatrix));

        if (computeMatrixPaletteData_) {
            computeMatrixPaletteData_[jointIndex].skeletonSpaceMatrix =
                Transpose(skinningMatrix);
            computeMatrixPaletteData_[jointIndex].skeletonSpaceInverseTransposeMatrix =
                Transpose(Inverse(skinningMatrix));
        }
    }

    for (size_t jointIndex = jointCount; jointIndex < kNumMaxSkeletonJoints; ++jointIndex) {
        skinningPaletteData_->palette[jointIndex].skeletonSpaceMatrix =
            MakeIdentity4x4();
        skinningPaletteData_->palette[jointIndex].skeletonSpaceInverseTransposeMatrix =
            MakeIdentity4x4();

        if (computeMatrixPaletteData_) {
            computeMatrixPaletteData_[jointIndex].skeletonSpaceMatrix =
                MakeIdentity4x4();
            computeMatrixPaletteData_[jointIndex].skeletonSpaceInverseTransposeMatrix =
                MakeIdentity4x4();
        }
    }
}

// ===== setter =====
void Object3d::SetScale(const Vector3& scale) {
    transform_.scale = scale;
}

void Object3d::SetRotate(const Vector3& rotate) {
    transform_.rotate = rotate;
    useQuaternionRotate_ = false;
}

void Object3d::SetQuaternionRotate(const Quaternion& rotate)
{
    quaternionRotate_ = NormalizeQuaternion(rotate);
    useQuaternionRotate_ = true;
}

void Object3d::SetTranslate(const Vector3& translate) {
    transform_.translate = translate;
}

void Object3d::SetDirectionalLightDirection(const Vector3& direction) {
    directionalLightData_->direction = direction;
}

void Object3d::SetDirectionalLightIntensity(float intensity) {
    directionalLightData_->intensity = intensity;
}

void Object3d::SetPointLightPosition(const Vector3& position) {
    pointLightData_->position = position;
}

void Object3d::SetPointLightIntensity(float intensity) {
    pointLightData_->intensity = intensity;
}

void Object3d::SetSpotLightPosition(const Vector3& position) {
    spotLightData_->position = position;
}

void Object3d::SetSpotLightDirection(const Vector3& direction) {
    spotLightData_->direction = Normalize(direction);
}

void Object3d::SetSpotLightIntensity(float intensity) {
    spotLightData_->intensity = intensity;
}

void Object3d::SetModel(Model* model)
{
    model_ = model;
    InitializeSkinning();
}

void Object3d::SetEnvironmentCoefficient(float coefficient)
{
    if (model_) {
        model_->SetEnvironmentCoefficient(coefficient);
    }
}

void Object3d::SetColor(const Vector4& color)
{
    if (model_) {
        model_->SetColor(color);
    }
}

void Object3d::SetAlphaReference(float alphaReference)
{
    if (model_) {
        model_->SetAlphaReference(alphaReference);
    }
}

void Object3d::SetUVTransform(const Matrix4x4& uvTransform)
{
    if (model_) {
        model_->SetUVTransform(uvTransform);
    }
}

void Object3d::SetLightingMode(int32_t lightingMode)
{
    if (model_) {
        model_->SetLightingMode(lightingMode);
    }
}

// ===== getter =====
Vector3 Object3d::GetScale() const {
    return transform_.scale;
}

Vector3 Object3d::GetRotate() const {
    return transform_.rotate;
}

Vector3 Object3d::GetTranslate() const {
    return transform_.translate;
}

float Object3d::GetEnvironmentCoefficient() const
{
    if (model_) {
        return model_->GetEnvironmentCoefficient();
    }
    return 0.0f;
}

void Object3d::SetModel(const std::string& filePath)
{
    model_ = ModelManager::GetInstance()->FindModel(filePath);
    InitializeSkinning();
}
