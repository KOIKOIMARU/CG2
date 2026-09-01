#include "engine/3d/Object3dCommon.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/Logger.h"
#include "engine/base/SrvManager.h"
#include <cassert>
#include <algorithm>
#include <array>
#include <cmath>

using Microsoft::WRL::ComPtr;
using Logger::Log;

namespace {

constexpr float kShadowLightDistance = 170.0f;
constexpr float kShadowViewHalfWidth = 145.0f;
constexpr float kShadowViewHalfHeight = 105.0f;
constexpr float kShadowNearClip = 1.0f;
constexpr float kShadowFarClip = 430.0f;

Math::Vector3 Subtract(const Math::Vector3& a, const Math::Vector3& b)
{
	return { a.x - b.x, a.y - b.y, a.z - b.z };
}

Math::Vector3 Scale(const Math::Vector3& v, float scale)
{
	return { v.x * scale, v.y * scale, v.z * scale };
}

float Dot(const Math::Vector3& a, const Math::Vector3& b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

Math::Vector3 Cross(const Math::Vector3& a, const Math::Vector3& b)
{
	return {
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x
	};
}

D3D12_CPU_DESCRIPTOR_HANDLE GetCpuDescriptorHandle(
	ID3D12DescriptorHeap* descriptorHeap,
	UINT descriptorSize,
	UINT index)
{
	assert(descriptorHeap);
	D3D12_CPU_DESCRIPTOR_HANDLE handle =
		descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += static_cast<SIZE_T>(descriptorSize) * index;
	return handle;
}

Math::Matrix4x4 MakeLookAtMatrix(
	const Math::Vector3& eye,
	const Math::Vector3& target,
	const Math::Vector3& up)
{
	const Math::Vector3 zAxis = Math::Normalize(Subtract(target, eye));
	Math::Vector3 xAxis = Math::Normalize(Cross(up, zAxis));
	if (std::abs(xAxis.x) + std::abs(xAxis.y) + std::abs(xAxis.z) <= 0.0001f) {
		xAxis = { 1.0f, 0.0f, 0.0f };
	}
	const Math::Vector3 yAxis = Cross(zAxis, xAxis);

	Math::Matrix4x4 result{};
	result.m[0][0] = xAxis.x;
	result.m[0][1] = yAxis.x;
	result.m[0][2] = zAxis.x;
	result.m[0][3] = 0.0f;
	result.m[1][0] = xAxis.y;
	result.m[1][1] = yAxis.y;
	result.m[1][2] = zAxis.y;
	result.m[1][3] = 0.0f;
	result.m[2][0] = xAxis.z;
	result.m[2][1] = yAxis.z;
	result.m[2][2] = zAxis.z;
	result.m[2][3] = 0.0f;
	result.m[3][0] = -Dot(xAxis, eye);
	result.m[3][1] = -Dot(yAxis, eye);
	result.m[3][2] = -Dot(zAxis, eye);
	result.m[3][3] = 1.0f;
	return result;
}

Math::Matrix4x4 MakeDirectionalLightViewProjection(
	const Math::Vector3& focusCenter,
	const Math::Vector3& lightDirection)
{
	const Math::Vector3 direction = Math::Normalize(lightDirection);
	const Math::Vector3 eye =
		Subtract(focusCenter, Scale(direction, kShadowLightDistance));
	const Math::Vector3 up =
		std::abs(direction.y) > 0.92f ?
		Math::Vector3{ 0.0f, 0.0f, 1.0f } :
		Math::Vector3{ 0.0f, 1.0f, 0.0f };
	const Math::Matrix4x4 view = MakeLookAtMatrix(eye, focusCenter, up);
	const Math::Matrix4x4 projection = Math::MakeOrthographicMatrix(
		-kShadowViewHalfWidth,
		kShadowViewHalfHeight,
		kShadowViewHalfWidth,
		-kShadowViewHalfHeight,
		kShadowNearClip,
		kShadowFarClip);
	return Math::Multiply(view, projection);
}

D3D12_BLEND_DESC MakeBlendDesc(BlendMode mode)
{
	D3D12_BLEND_DESC desc{};
	auto& rt = desc.RenderTarget[0];
	rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	rt.BlendEnable = TRUE;
	rt.BlendOp = D3D12_BLEND_OP_ADD;
	rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;

	switch (mode) {
	case BlendMode::None:
		rt.BlendEnable = FALSE;
		rt.SrcBlend = D3D12_BLEND_ONE;
		rt.DestBlend = D3D12_BLEND_ZERO;
		rt.SrcBlendAlpha = D3D12_BLEND_ONE;
		rt.DestBlendAlpha = D3D12_BLEND_ZERO;
		break;
	case BlendMode::Normal:
		rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		rt.SrcBlendAlpha = D3D12_BLEND_ONE;
		rt.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		break;
	case BlendMode::Add:
		rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		rt.DestBlend = D3D12_BLEND_ONE;
		rt.SrcBlendAlpha = D3D12_BLEND_ONE;
		rt.DestBlendAlpha = D3D12_BLEND_ONE;
		break;
	case BlendMode::Subtract:
		rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		rt.DestBlend = D3D12_BLEND_ONE;
		rt.BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
		rt.SrcBlendAlpha = D3D12_BLEND_ONE;
		rt.DestBlendAlpha = D3D12_BLEND_ONE;
		rt.BlendOpAlpha = D3D12_BLEND_OP_REV_SUBTRACT;
		break;
	case BlendMode::Multiply:
		rt.SrcBlend = D3D12_BLEND_DEST_COLOR;
		rt.DestBlend = D3D12_BLEND_ZERO;
		rt.SrcBlendAlpha = D3D12_BLEND_DEST_ALPHA;
		rt.DestBlendAlpha = D3D12_BLEND_ZERO;
		break;
	case BlendMode::Screen:
		rt.SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
		rt.DestBlend = D3D12_BLEND_ONE;
		rt.SrcBlendAlpha = D3D12_BLEND_ONE;
		rt.DestBlendAlpha = D3D12_BLEND_INV_DEST_ALPHA;
		break;
	case BlendMode::Count:
		assert(false);
		break;
	}

	return desc;
}

}

void Object3dCommon::Initialize(
	DirectXCommon* dxCommon,
	SrvManager* srvManager)
{
	assert(dxCommon);
	assert(srvManager);

	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	shadowLightDirection_ = Math::Normalize(shadowLightDirection_);

	CreateRootSignature();
	CreateGraphicsPipelineState();
	CreateShadowMap();
}

void Object3dCommon::CreateRootSignature() {
    auto* device = dxCommon_->GetDevice();
    HRESULT hr = S_OK;

	// RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// RootParameter作成
	D3D12_ROOT_PARAMETER rootParameters[11] = {};

	// b0: MaterialCB (PixelShader)
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	// b1: TransformCB (VertexShader)
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].Descriptor.ShaderRegister = 1;

	// b2: CameraCB (PixelShader)
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].Descriptor.ShaderRegister = 2;

	// t0: SRVテクスチャ (PixelShader)
	D3D12_DESCRIPTOR_RANGE descriptorRange[4] = {};
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	descriptorRange[1].BaseShaderRegister = 1;
	descriptorRange[1].NumDescriptors = 1;
	descriptorRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	descriptorRange[2].BaseShaderRegister = 2;
	descriptorRange[2].NumDescriptors = 1;
	descriptorRange[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	descriptorRange[3].BaseShaderRegister = 3;
	descriptorRange[3].NumDescriptors = 1;
	descriptorRange[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[3].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].DescriptorTable.pDescriptorRanges = &descriptorRange[0];
	rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;

	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[4].DescriptorTable.pDescriptorRanges = &descriptorRange[1];
	rootParameters[4].DescriptorTable.NumDescriptorRanges = 1;

	// b3: DirectionalLight (PixelShader)
	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[5].Descriptor.ShaderRegister = 3;

	// b4: PointLight (PixelShader)
	rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[6].Descriptor.ShaderRegister = 4;

	// b5: SpotLight (PixelShader)
	rootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[7].Descriptor.ShaderRegister = 5;

	// b6: SkinningPalette (VertexShader)
	rootParameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[8].Descriptor.ShaderRegister = 6;

	rootParameters[9].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[9].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[9].DescriptorTable.pDescriptorRanges = &descriptorRange[2];
	rootParameters[9].DescriptorTable.NumDescriptorRanges = 1;

	rootParameters[10].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[10].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[10].DescriptorTable.pDescriptorRanges = &descriptorRange[3];
	rootParameters[10].DescriptorTable.NumDescriptorRanges = 1;

	// ルートシグネチャのセットアップ
	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);

	// Samplerの設定
	D3D12_STATIC_SAMPLER_DESC staticSamplers[2] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;   // 0~1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;   // 比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;   // ありったけのMipmapを使う
	staticSamplers[0].ShaderRegister = 0;   // レジスタ番号0を使う
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	staticSamplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	staticSamplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[1].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[1].ShaderRegister = 1;
	staticSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);



	// シリアライズしてバイナリにする
	ID3DBlob* signatureBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}
	// バイナリを元に生成
	ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	hr = device->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature_)
	);
	assert(SUCCEEDED(hr));

}

void Object3dCommon::CreateGraphicsPipelineState() {
    auto* device = dxCommon_->GetDevice();
    HRESULT hr = S_OK;

	// InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		// POSITION (float4)
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

			// TEXCOORD (float2)
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
				D3D12_APPEND_ALIGNED_ELEMENT,
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

				// NORMAL (float3)
				{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
					D3D12_APPEND_ALIGNED_ELEMENT,
					D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

				// BLENDWEIGHT (float4)
				{ "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
					D3D12_APPEND_ALIGNED_ELEMENT,
					D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

				// BLENDINDICES (uint4)
				{ "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0,
					D3D12_APPEND_ALIGNED_ELEMENT,
					D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs; // セマンティクスの情報
	inputLayoutDesc.NumElements = _countof(inputElementDescs); // セマンティクスの数

	// RasterizerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	// 裏面を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// Shaderのコンパイル
	auto vertexShaderBlob =
		dxCommon_->CompileShader(L"shaders/Object3D.VS.hlsl", L"vs_6_0");

	auto pixelShaderBlob =
		dxCommon_->CompileShader(L"shaders/Object3D.PS.hlsl", L"ps_6_0");

	auto shadowVertexShaderBlob =
		dxCommon_->CompileShader(L"shaders/Object3DShadow.VS.hlsl", L"vs_6_0");


	// PSOを生成する
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc; // InputLayout
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() }; // VertexShader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() }; // PixelShader
	graphicsPipelineStateDesc.BlendState = MakeBlendDesc(BlendMode::Normal); // BlendState
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc; // RasterizerState
	// 書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	// 利用するトポロジ（形状）のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// どのように画面に色を打ち込むかの設定（気にしなくていい）
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	// Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;
	// 書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	// 比較関数はLessEqual。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	// DepthStencilの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// DepthDrawMode / BlendMode ごとに PSO を生成
	for (uint32_t depthModeIndex = 0;
		depthModeIndex < static_cast<uint32_t>(DepthDrawMode::Count);
		++depthModeIndex) {
		const DepthDrawMode depthDrawMode =
			static_cast<DepthDrawMode>(depthModeIndex);
		if (depthDrawMode == DepthDrawMode::Overlay) {
			graphicsPipelineStateDesc.DepthStencilState.DepthWriteMask =
				D3D12_DEPTH_WRITE_MASK_ZERO;
			graphicsPipelineStateDesc.DepthStencilState.DepthFunc =
				D3D12_COMPARISON_FUNC_ALWAYS;
		} else if (depthDrawMode == DepthDrawMode::ReadOnly) {
			graphicsPipelineStateDesc.DepthStencilState.DepthWriteMask =
				D3D12_DEPTH_WRITE_MASK_ZERO;
			graphicsPipelineStateDesc.DepthStencilState.DepthFunc =
				D3D12_COMPARISON_FUNC_LESS_EQUAL;
		} else {
			graphicsPipelineStateDesc.DepthStencilState.DepthWriteMask =
				D3D12_DEPTH_WRITE_MASK_ALL;
			graphicsPipelineStateDesc.DepthStencilState.DepthFunc =
				D3D12_COMPARISON_FUNC_LESS_EQUAL;
		}

		for (uint32_t blendModeIndex = 0;
			blendModeIndex < static_cast<uint32_t>(BlendMode::Count);
			++blendModeIndex) {
			graphicsPipelineStateDesc.BlendState =
				MakeBlendDesc(static_cast<BlendMode>(blendModeIndex));

			hr = device->CreateGraphicsPipelineState(
				&graphicsPipelineStateDesc,
				IID_PPV_ARGS(&pipelineStates_[depthModeIndex][blendModeIndex])
			);
			assert(SUCCEEDED(hr));
		}
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPipelineStateDesc =
		graphicsPipelineStateDesc;
	shadowPipelineStateDesc.VS = {
		shadowVertexShaderBlob->GetBufferPointer(),
		shadowVertexShaderBlob->GetBufferSize()
	};
	shadowPipelineStateDesc.PS = {};
	shadowPipelineStateDesc.BlendState = MakeBlendDesc(BlendMode::None);
	shadowPipelineStateDesc.NumRenderTargets = 0;
	for (DXGI_FORMAT& format : shadowPipelineStateDesc.RTVFormats) {
		format = DXGI_FORMAT_UNKNOWN;
	}
	shadowPipelineStateDesc.DepthStencilState.DepthEnable = true;
	shadowPipelineStateDesc.DepthStencilState.DepthWriteMask =
		D3D12_DEPTH_WRITE_MASK_ALL;
	shadowPipelineStateDesc.DepthStencilState.DepthFunc =
		D3D12_COMPARISON_FUNC_LESS_EQUAL;
	shadowPipelineStateDesc.RasterizerState.DepthBias = 1600;
	shadowPipelineStateDesc.RasterizerState.SlopeScaledDepthBias = 1.35f;
	shadowPipelineStateDesc.RasterizerState.DepthBiasClamp = 0.0f;
	shadowPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	hr = device->CreateGraphicsPipelineState(
		&shadowPipelineStateDesc,
		IID_PPV_ARGS(&shadowPipelineState_)
	);
	assert(SUCCEEDED(hr));

}

void Object3dCommon::CommonDrawSetting() {
	auto* commandList = dxCommon_->GetCommandList();

	// ❌ 削除する
	// ID3D12DescriptorHeap* heaps[] = { srvManager_->GetDescriptorHeap() };
	// commandList->SetDescriptorHeaps(1, heaps);

	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(
		pipelineStates_[static_cast<size_t>(depthDrawMode_)]
			[static_cast<size_t>(blendMode_)].Get());
	commandList->IASetPrimitiveTopology(
		D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Object3dCommon::CommonShadowDrawSetting()
{
	auto* commandList = dxCommon_->GetCommandList();
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(shadowPipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Object3dCommon::CreateShadowMap()
{
	auto* device = dxCommon_->GetDevice();
	assert(device);
	assert(srvManager_);

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Width = kShadowMapSize;
	resourceDesc.Height = kShadowMapSize;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_CLEAR_VALUE clearValue{};
	clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0;

	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		shadowMapState_,
		&clearValue,
		IID_PPV_ARGS(&shadowMapResource_));
	assert(SUCCEEDED(hr));

	shadowMapDsvHeap_ = dxCommon_->CreateDescriptorHeap(
		D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
		1,
		false);

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	device->CreateDepthStencilView(
		shadowMapResource_.Get(),
		&dsvDesc,
		shadowMapDsvHeap_->GetCPUDescriptorHandleForHeapStart());

	shadowMapSrvIndex_ = srvManager_->Allocate();
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.Shader4ComponentMapping =
		D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	device->CreateShaderResourceView(
		shadowMapResource_.Get(),
		&srvDesc,
		srvManager_->GetCPUDescriptorHandle(shadowMapSrvIndex_));

	shadowViewport_.Width = static_cast<float>(kShadowMapSize);
	shadowViewport_.Height = static_cast<float>(kShadowMapSize);
	shadowViewport_.MinDepth = 0.0f;
	shadowViewport_.MaxDepth = 1.0f;
	shadowScissorRect_.left = 0;
	shadowScissorRect_.top = 0;
	shadowScissorRect_.right = static_cast<LONG>(kShadowMapSize);
	shadowScissorRect_.bottom = static_cast<LONG>(kShadowMapSize);
}

void Object3dCommon::TransitionShadowMap(D3D12_RESOURCE_STATES nextState)
{
	if (!shadowMapResource_ || shadowMapState_ == nextState) {
		return;
	}

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = shadowMapResource_.Get();
	barrier.Transition.StateBefore = shadowMapState_;
	barrier.Transition.StateAfter = nextState;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
	shadowMapState_ = nextState;
}

bool Object3dCommon::BeginShadowPass(const Math::Vector3& focusCenter)
{
	if (!shadowMapResource_ || !shadowMapDsvHeap_ || !shadowPipelineState_) {
		return false;
	}

	shadowMapReady_ = false;
	shadowLightViewProjection_ =
		MakeDirectionalLightViewProjection(focusCenter, shadowLightDirection_);

	TransitionShadowMap(D3D12_RESOURCE_STATE_DEPTH_WRITE);

	auto* commandList = dxCommon_->GetCommandList();
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
		shadowMapDsvHeap_->GetCPUDescriptorHandleForHeapStart();
	commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);
	commandList->ClearDepthStencilView(
		dsvHandle,
		D3D12_CLEAR_FLAG_DEPTH,
		1.0f,
		0,
		0,
		nullptr);
	commandList->RSSetViewports(1, &shadowViewport_);
	commandList->RSSetScissorRects(1, &shadowScissorRect_);
	CommonShadowDrawSetting();
	return true;
}

void Object3dCommon::EndShadowPass()
{
	TransitionShadowMap(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	shadowMapReady_ = shadowMapResource_ && shadowMapSrvIndex_ != UINT32_MAX;
	RestoreMainRenderTarget();
}

void Object3dCommon::RestoreMainRenderTarget()
{
	auto* commandList = dxCommon_->GetCommandList();
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = GetCpuDescriptorHandle(
		dxCommon_->GetRTVHeap(),
		dxCommon_->GetRTVDescriptorSize(),
		DirectXCommon::kRenderTextureRTVIndex);
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
		dxCommon_->GetDSVHeap()->GetCPUDescriptorHandleForHeapStart();

	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	const D3D12_VIEWPORT& viewport = dxCommon_->GetViewport();
	const D3D12_RECT& scissorRect = dxCommon_->GetScissorRect();
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect);
}
