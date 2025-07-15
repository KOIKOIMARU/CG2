#define NOMINMAX
#include <Windows.h>
#include <cstdint>
#include <vector>
#include <cmath>
#include <string>
#include <queue>
#include <format>
#include <random> 
#include <d3d12.h>
#include <d3dcompiler.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <dxcapi.h>
#include <cassert>
#include <fstream>
#include <sstream>
#include <wrl.h>
#include "ResourceObject.h"
#include <wrl/client.h>
#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定
#include <dinput.h>
#include "MathTypes.h"
#include "DebugCamera.h"
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "externals/DirectXTex/DirectXTex.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

using namespace Microsoft::WRL;


struct VertexData {
	Vector4 position; // 頂点の位置
	Vector2 texcoord; // テクスチャ座標
	Vector3 normal;   // 法線ベクトル
};

struct Material {
	Vector4 color;
	int32_t enableLighting;
	float padding[3]; // ← 16バイトアライメントのため
	Matrix4x4 uvTransform;
};


struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
};

struct DirectionalLight {
	Vector4 color;     // 光の色
	Vector4 direction; // 光の方向
	float intensity;   // 光の強度
	float padding[3];
};

struct ModelData {
	std::vector<VertexData> vertices;
	std::vector<uint32_t> indices;
	Material material;

	ComPtr<ID3D12Resource> vertexResource;
	ComPtr<ID3D12Resource> indexResource;

};


struct MaterialData {
	std::string textureFilePath;
};

struct FragmentModel {
	std::vector<VertexData> vertices;
	std::vector<uint32_t> indices;
};
std::vector<FragmentModel> fragments;

struct GlassFragment {
	ModelData model;       // 頂点・インデックス情報
	Transform transform;   // 位置・回転・スケール
	Vector3 velocity;      // 飛び出す方向と速度
	bool isFlying = false; // 飛散中フラグ
	ComPtr<ID3D12Resource> wvpResource;
};


struct D3DResourceLeakChecker {
	~D3DResourceLeakChecker() {
		// リソースリークチェック
		ComPtr<IDXGIDebug1> debug;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
			debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
		}
	}
};

// チャンクヘッダ
struct ChunkHeader {
	char id[4];     // チャンク毎のID
	int32_t size;   // チャンクサイズ
};

static void Log(const std::string& message) {
	OutputDebugStringA(message.c_str());
}

// stringをwstringに変換する関数
static std::wstring ConvertString(const std::string& str) {
	if (str.empty()) {
		return std::wstring();
	}

	auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), NULL, 0);
	if (sizeNeeded == 0) {
		return std::wstring();
	}
	std::wstring result(sizeNeeded, 0);
	MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);
	return result;
}

// wstringをstringに変換する関数
static std::string ConvertString(const std::wstring& str) {
	if (str.empty()) {
		return std::string();
	}

	auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
	if (sizeNeeded == 0) {
		return std::string();
	}
	std::string result(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), sizeNeeded, NULL, NULL);
	return result;
}

// ウィンドウプロシージャ
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {

	// ImGuiのウィンドウプロシージャを呼び出す
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}

	// メッセージに応じてゲーム固有の処理を行う
	switch (msg) {
		// ウィンドウが破壊された
	case WM_DESTROY:
		// ウィンドウに対してアプリの終了を伝える
		PostQuitMessage(0);
		return 0;
	}

	// 標準のメッセージ処理を行う
	return DefWindowProc(hwnd, msg, wparam, lparam);

}

// 関数の作成

IDxcBlob* CompileShader(
	// CompilerするShaderファイルへのパス
	const std::wstring& filePath,
	// Compileに使用するProfile
	const wchar_t* profile,
	// 初期化で生成したものを3つ
	IDxcUtils* dxcUtils,
	IDxcCompiler3* dxcCompiler,
	IDxcIncludeHandler* includeHandler) {
	// 1.hlslファイルを読む
	// これからシェーダーをコンパイルする旨をログに出す
	Log(ConvertString(std::format(L"Begin CompileShader, path:{}, profile:{}\n", filePath, profile)));
	// hlslファイルを読む
	IDxcBlobEncoding* shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	// 読めなかったら止める
	assert(SUCCEEDED(hr));
	// 読み込んだファイルの内容を設定する
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;  // UTF8の文字コードであることを通知

	// 2.Compileする
	LPCWSTR arguments[] = {
		filePath.c_str(), // コンパイル対象のhlslファイル名
		L"-E", L"main", // エントリーポイントの指定。基本的にmain以外には市内
		L"-T", profile, // ShaderProfileの設定
		L"-Zi", L"-Qembed_debug",   // デバッグ用の情報を埋め込む
		L"-Od",     // 最適化を外しておく
		L"-Zpr",     // メモリレイアウトは行優先
	};
	// 実際にShaderをコンパイルする
	IDxcResult* shaderResult = nullptr;
	hr = dxcCompiler->Compile(
		&shaderSourceBuffer, // 読み込んだファイル
		arguments,           // コンパイルオプション
		_countof(arguments),  // コンパイルオプションの数
		includeHandler,      // includeが含まれた諸々
		IID_PPV_ARGS(&shaderResult) // コンパイル結果
	);
	// コンパイルエラーではなくdxcが起動できないなど致命的な状況
	assert(SUCCEEDED(hr));

	// 3.警告・エラーがでていないか確認する
	// 警告・エラーが出てたらログに出して止める
	IDxcBlobUtf8* shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Log(shaderError->GetStringPointer());
	}

	// 4.Compile結果を受け取って返す
	// コンパイル結果から実行用のバイナリ部分を取得
	IDxcBlob* shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));
	// 成功したログを出す
	Log(ConvertString(std::format(L"CompileSucceded, path:{}, profile:{}\n", filePath, profile)));
	// 実行用のバイナリを返却
	return shaderBlob;
}


ComPtr<ID3D12Resource> CreateBufferResource(ComPtr<ID3D12Device>& device, size_t sizeInBytes) {
	// ヒープ設定（UploadHeap）
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	// リソース設定（バッファ用）
	D3D12_RESOURCE_DESC vertexResourceDesc{};
	vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexResourceDesc.Width = sizeInBytes;
	vertexResourceDesc.Height = 1;
	vertexResourceDesc.DepthOrArraySize = 1;
	vertexResourceDesc.MipLevels = 1;
	vertexResourceDesc.SampleDesc.Count = 1;
	vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// リソース作成
	ComPtr<ID3D12Resource> vertexResource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&vertexResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertexResource)
	);
	assert(SUCCEEDED(hr));

	return vertexResource;
}

ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ComPtr<ID3D12Device>& device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible) {
	// ディスクリプタヒープの生成
	ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
	descriptorHeapDesc.Type = heapType; // レンダーターゲットビュー用
	descriptorHeapDesc.NumDescriptors = numDescriptors; // 多くても別に構わない
	descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
	// ディスクリプタヒープの生成が作れなかったので起動できない
	assert(SUCCEEDED(hr));
	return descriptorHeap;
}

static DirectX::ScratchImage LoadTexture(const std::string& filePath) {
	// テクスチャファイルを読んでプログラムで扱えるようにする
	DirectX::ScratchImage image{};
	std::wstring filePathW = ConvertString(filePath);
	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, image);
	assert(SUCCEEDED(hr)); // テクスチャの読み込みに失敗したらエラー

	// ミップマップの作成
	DirectX::ScratchImage mipImages{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_DEFAULT, 0, mipImages);
	assert(SUCCEEDED(hr)); // ミップマップの生成に失敗したらエラー

	return mipImages;

}

static ComPtr<ID3D12Resource> CreateTextureResource(ComPtr<ID3D12Device>& device, const DirectX::TexMetadata& metadata) {
	// metadateを基にResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width);
	resourceDesc.Height = UINT(metadata.height); resourceDesc.MipLevels = UINT(metadata.mipLevels);
	resourceDesc.DepthOrArraySize = UINT(metadata.arraySize);
	resourceDesc.Format = metadata.format;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);
	// 利用するヒープの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	// Resourceの生成
	ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr)); // テクスチャリソースの生成に失敗したらエラー
	// テクスチャリソースの生成に成功したら、リソースを返す
	return resource;

};

static void UploadTextureData(
	ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages) {
	// Meta情報を取得
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	// 全mipmapについて
	for (size_t mipLevel = 0; mipLevel < metadata.mipLevels; ++mipLevel) {
		// mipmapの情報を取得
		const DirectX::Image* img = mipImages.GetImage(mipLevel, 0, 0);
		HRESULT hr = texture->WriteToSubresource(
			UINT(mipLevel), // mipmapレベル
			0, // mipmapの最初のレベル
			img->pixels, // ピクセルデータ
			UINT(img->rowPitch), // 行のピッチ
			UINT(img->slicePitch) // スライスのピッチ
		);
		assert(SUCCEEDED(hr)); // テクスチャデータのアップロードに失敗したらエラー
	}
}

static ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(
	ComPtr<ID3D12Device>& device, int32_t width, int32_t height) {
	// 生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;
	resourceDesc.Height = height;
	resourceDesc.MipLevels = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // 深度24bit、ステンシル8bit
	resourceDesc.SampleDesc.Count = 1; // MSAAは使用しない
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // 深度ステンシル用のフラグを設定

	// ヒーププロパティの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // デフォルトヒープを使用

	// 深度値の設定
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f; // 深度値の初期値
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // 深度フォーマット

	// リソースの生成
	ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, // 深度書き込み用の初期状態
		&depthClearValue, // 深度値の初期値
		IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr)); // 深度ステンシルテクスチャの生成に失敗したらエラー
	return resource;

}

D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ComPtr<ID3D12DescriptorHeap>& descriptorHeap, UINT descriptorSize, UINT index) {
	// ディスクリプタヒープのCPUハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ComPtr<ID3D12DescriptorHeap>& descriptorHeap, UINT descriptorSize, UINT index) {
	// ディスクリプタヒープのGPUハンドルを取得
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize * index);
	return handleGPU;
}

void UploadBufferData(ID3D12Resource* buffer, const void* data, size_t size) {
	void* mapped = nullptr;
	buffer->Map(0, nullptr, &mapped);
	memcpy(mapped, data, size);
	buffer->Unmap(0, nullptr);
}


void SetVertex(VertexData& v, const Vector4& pos, const Vector2& uv) {
	v.position = pos;
	v.texcoord = uv;
	Vector3 p = { pos.x, pos.y, pos.z };
	v.normal = Normalize(p);
}

ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename) {
	ModelData modelData;
	std::vector<Vector4> positions;
	std::vector<Vector2> texcoords;
	std::vector<Vector3> normals;

	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open());

	std::string line;
	while (std::getline(file, line)) {
		std::istringstream iss(line);
		std::string prefix;
		iss >> prefix;

		if (prefix == "v") {
			Vector4 pos;
			iss >> pos.x >> pos.y >> pos.z;
			pos.w = 1.0f;
			pos.x *= -1.0f;
			positions.push_back(pos);
		} else if (prefix == "vt") {
			Vector2 uv;
			iss >> uv.x >> uv.y;
			texcoords.push_back({ uv.x, 1.0f - uv.y });
		} else if (prefix == "vn") {
			Vector3 normal;
			iss >> normal.x >> normal.y >> normal.z;
			normal.x *= -1.0f;
			normals.push_back(normal);
		} else if (prefix == "f") {
			for (int i = 0; i < 3; ++i) {
				std::string vtn;
				iss >> vtn;
				size_t s1 = vtn.find('/');
				size_t s2 = vtn.find('/', s1 + 1);

				int vi = std::stoi(vtn.substr(0, s1)) - 1;
				int ti = -1, ni = -1;
				if (s2 != std::string::npos) {
					if (s2 > s1 + 1) {
						ti = std::stoi(vtn.substr(s1 + 1, s2 - s1 - 1)) - 1;
					}
					if (s2 + 1 < vtn.length()) {
						ni = std::stoi(vtn.substr(s2 + 1)) - 1;
					}
				}

				if (vi >= 0 && vi < (int)positions.size()) {
					VertexData v;
					v.position = positions[vi];
					v.texcoord = (ti >= 0 && ti < (int)texcoords.size()) ? texcoords[ti] : Vector2{ 0, 0 };
					v.normal = (ni >= 0 && ni < (int)normals.size()) ? normals[ni] : Vector3{ 0, 1, 0 };

					modelData.indices.push_back((uint32_t)modelData.vertices.size());
					modelData.vertices.push_back(v);
				}
			}
		}
	}

	return modelData;
}




void InitializeGlassFragmentBuffers(ComPtr<ID3D12Device>& device, std::vector<GlassFragment>& fragments) {
	for (auto& frag : fragments) {
		frag.model.vertexResource = CreateBufferResource(device, frag.model.vertices.size() * sizeof(VertexData));
		void* vertexData = nullptr;
		frag.model.vertexResource->Map(0, nullptr, &vertexData);
		memcpy(vertexData, frag.model.vertices.data(), frag.model.vertices.size() * sizeof(VertexData));

		frag.model.indexResource = CreateBufferResource(device, frag.model.indices.size() * sizeof(uint32_t));
		void* indexData = nullptr;
		frag.model.indexResource->Map(0, nullptr, &indexData);
		memcpy(indexData, frag.model.indices.data(), frag.model.indices.size() * sizeof(uint32_t));
	}
}

void UpdateAndDrawFragments(
	ID3D12GraphicsCommandList* commandList,
	const std::vector<GlassFragment>& fragments,
	ComPtr<ID3D12Resource>& materialResource,
	ComPtr<ID3D12Resource>& lightResource,
	D3D12_GPU_DESCRIPTOR_HANDLE textureHandle,
	const Matrix4x4& viewMatrix,
	const Matrix4x4& projectionMatrix
) {
	Material* materialData = nullptr;
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));

	for (const auto& frag : fragments) {
		// 個別のWVPバッファへ書き込み
		TransformationMatrix* wvpData = nullptr;
		frag.wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));

		Matrix4x4 world = MakeAffineMatrix(frag.transform.scale, frag.transform.rotate, frag.transform.translate);
		Matrix4x4 wvp = multiplayMatrix(world, multiplayMatrix(viewMatrix, projectionMatrix));
		wvpData->World = world;
		wvpData->WVP = wvp;

		frag.wvpResource->Unmap(0, nullptr);

		// ルート定数バッファ設定
		commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
		commandList->SetGraphicsRootConstantBufferView(1, frag.wvpResource->GetGPUVirtualAddress());
		commandList->SetGraphicsRootConstantBufferView(3, lightResource->GetGPUVirtualAddress());
		commandList->SetGraphicsRootDescriptorTable(2, textureHandle);

		// VB/IB設定
		D3D12_VERTEX_BUFFER_VIEW vbView{};
		vbView.BufferLocation = frag.model.vertexResource->GetGPUVirtualAddress();
		vbView.SizeInBytes = static_cast<UINT>(frag.model.vertices.size() * sizeof(VertexData));
		vbView.StrideInBytes = sizeof(VertexData);
		commandList->IASetVertexBuffers(0, 1, &vbView);

		D3D12_INDEX_BUFFER_VIEW ibView{};
		ibView.BufferLocation = frag.model.indexResource->GetGPUVirtualAddress();
		ibView.SizeInBytes = static_cast<UINT>(frag.model.indices.size() * sizeof(uint32_t));
		ibView.Format = DXGI_FORMAT_R32_UINT;
		commandList->IASetIndexBuffer(&ibView);

		// 描画
		commandList->DrawIndexedInstanced(static_cast<UINT>(frag.model.indices.size()), 1, 0, 0, 0);
	}
}

std::vector<GlassFragment> GenerateSphereFragmentsFromSingleModel(
	ComPtr<ID3D12Device>& device,
	const std::string& filepath,
	int count,
	float radius = 0.2f
) {
	ModelData baseModel = LoadObjFile("resources", filepath);

	std::vector<GlassFragment> fragments;
	fragments.reserve(count);

	const float offset = 2.0f / count;
	const float increment = 3.1415926f * (3.0f - std::sqrt(5.0f)); // ゴールデンアングル

	for (int i = 0; i < count; ++i) {
		float y = ((i * offset) - 1.0f) + (offset / 2.0f);
		float r = std::sqrt(1.0f - y * y);

		float phi = i * increment;

		float x = std::cos(phi) * r;
		float z = std::sin(phi) * r;

		GlassFragment frag;
		frag.model = baseModel;

		frag.model.vertexResource = CreateBufferResource(device, sizeof(VertexData) * baseModel.vertices.size());
		frag.model.indexResource = CreateBufferResource(device, sizeof(uint32_t) * baseModel.indices.size());

		UploadBufferData(frag.model.vertexResource.Get(), baseModel.vertices.data(), sizeof(VertexData) * baseModel.vertices.size());
		UploadBufferData(frag.model.indexResource.Get(), baseModel.indices.data(), sizeof(uint32_t) * baseModel.indices.size());

		Vector3 normal = Normalize(Vector3{ x, y, z });

		// オイラー角でZ軸をnormal方向へ向ける
		float pitch = std::asin(-normal.y);
		float yaw = std::atan2(normal.x, normal.z);
		float roll = 0.0f;

		frag.wvpResource = CreateBufferResource(device, sizeof(TransformationMatrix));

		frag.transform.scale = { 0.3f, 0.3f, 0.3f };
	
		frag.transform.rotate = { pitch, yaw, roll };

		frag.transform.translate = { x * radius, y * radius, z * radius + 5 };
		frag.velocity = { 0, 0, 0 };
		frag.isFlying = false;

		fragments.push_back(frag);
	}

	return fragments;
}





// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	D3DResourceLeakChecker leakcheck;

	CoInitializeEx(0, COINIT_MULTITHREADED);

	// ウィンドウクラスの定義
	WNDCLASS wc = {};
	// ウィンドウプロシージャ
	wc.lpfnWndProc = WindowProc;
	// ウィンドウクラス名
	wc.lpszClassName = L"CG2WindowClass";
	// インスタンスハンドル
	wc.hInstance = GetModuleHandle(nullptr);
	// カーソル
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	// ウィンドウクラスの登録
	RegisterClass(&wc);

	// クライアント領域のサイズ
	const int32_t kClientWidth = 1280;
	const int32_t kClientHeight = 720;

	// ウィンドウサイズを表す構造体にクライアント領域を入れる
	RECT wrc = { 0, 0, kClientWidth, kClientHeight };

	// クライアント領域を元に実際のサイズにwrcを変更してもらう
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	// ウィンドウの作成
	HWND hwnd = CreateWindow(
		wc.lpszClassName, // ウィンドウクラス名
		L"CG2", // ウィンドウ名
		WS_OVERLAPPEDWINDOW, // ウィンドウスタイル
		CW_USEDEFAULT, // 表示X座標(Windowsに任せる
		CW_USEDEFAULT, // 表示Y座標
		wrc.right - wrc.left, // ウィンドウ横幅
		wrc.bottom - wrc.top, // ウィンドウ縦幅
		nullptr, // 親ウィンドウハンドル
		nullptr, // メニューハンドル
		wc.hInstance, // インスタンスハンドル
		nullptr); // オプション

	// ウィンドウを表示する
	ShowWindow(hwnd, SW_SHOW);

#ifdef _DEBUG
	ComPtr<ID3D12Debug1> debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		// デバッグレイヤーを有効にする
		debugController->EnableDebugLayer();
		// さらにGPU側でもチェックを行う
		debugController->SetEnableGPUBasedValidation(true);
	}
#endif

	// DXGIファクトリーの生成
	ComPtr<IDXGIFactory7> dxgiFactory = nullptr;
	// 関数が成功したかどうかの判定
	HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
	assert(SUCCEEDED(hr));

	// 使用するアダプタ用の変数
	ComPtr<IDXGIAdapter4> useAdapter = nullptr;
	// 良い順にアダプタを探す
	for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i,
		DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) !=
		DXGI_ERROR_NOT_FOUND; ++i) {
		// アダプタの情報を取得する
		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = useAdapter->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr)); // 取得に失敗したらエラー
		// ソフトウェアアダプタでなければ採用
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
			break;
		}
		useAdapter = nullptr; // ソフトウェアアダプタの場合はnullptrにする
	}
	// 適切なアダプタが見つからなかった場合はエラー
	assert(useAdapter != nullptr);

	ComPtr<ID3D12Device> device = nullptr;
	// 機能レベルとログ出力用の文字列
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2,D3D_FEATURE_LEVEL_12_1,D3D_FEATURE_LEVEL_12_0 };

	const char* featureLevelStrings[] = { "12.2","12.1","12.0" };
	// 高い順に生成できるか試していく
	for (size_t i = 0; i < _countof(featureLevels); ++i) {
		// 採用したアダプタでデバイス生成
		hr = D3D12CreateDevice(useAdapter.Get(), featureLevels[i], IID_PPV_ARGS(&device));
		// 指定した機能レベルでデバイスが生成できたか
		if (SUCCEEDED(hr)) {
			// 生成できたのでログ出力を行ってループを抜ける
			Log(std::format("Feature Level: {}\n",
				featureLevelStrings[i]));
			break;
		}
	}
	// デバイスの生成が上手くいかなかったので起動できない
	assert(device != nullptr);
	Log("Complete create D3D12Device!!!\n");// 初期化完了のログを出す

#ifdef _DEBUG
	ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
		// ヤバいエラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		// エラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		// 警告時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

		// 抑制するメッセージのID
		D3D12_MESSAGE_ID denyIds[] = {
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};
		// 抑制するレベル
		D3D12_MESSAGE_SEVERITY severities[] = {
			D3D12_MESSAGE_SEVERITY_INFO
		};
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		// 指定したメッセージの表示を抑制する
		infoQueue->PushStorageFilter(&filter);
	}

#endif

	// コマンドキューを生成する
	ComPtr<ID3D12CommandQueue> commandQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	hr = device->CreateCommandQueue(&commandQueueDesc,
		IID_PPV_ARGS(&commandQueue));
	assert(SUCCEEDED(hr)); // コマンドキューの生成に失敗したらエラー

	// コマンドアロケータを生成する
	ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr;
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&commandAllocator));
	assert(SUCCEEDED(hr)); // コマンドアロケータの生成に失敗したらエラー

	// コマンドリストを生成する
	ComPtr<ID3D12GraphicsCommandList> commandList = nullptr;
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
		commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList));
	assert(SUCCEEDED(hr)); // コマンドリストの生成に失敗したらエラー

	// スワップチェーンを生成する
	ComPtr<IDXGISwapChain4> swapChain = nullptr;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = kClientWidth;     // 画面の幅。ウィンドウのクライアント領域を同じものにしておく
	swapChainDesc.Height = kClientHeight;   // 画面の高さ。ウィンドウのクライアント領域を同じものにしておく
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;  // 色の形式
	swapChainDesc.SampleDesc.Count = 1; // マルチサンプルしない
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 描画のターゲットとして利用する
	swapChainDesc.BufferCount = 2; // ダブルバッファ
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // モニタに移したら、中身を破棄
	// コマンドキュー、ウィンドウハンドル、設定を渡して生成する	
	hr = dxgiFactory->CreateSwapChainForHwnd(
		commandQueue.Get(), // コマンドキュー
		hwnd, // ウィンドウハンドル
		&swapChainDesc, // スワップチェーンの設定
		nullptr, // モニタのハンドル
		nullptr, // フルスクリーンモードの設定
		reinterpret_cast<IDXGISwapChain1**>(swapChain.GetAddressOf())); // スワップチェーンのポインタ
	assert(SUCCEEDED(hr)); // スワップチェーンの生成に失敗したらエラー

	// DepthStencilTextureをウィンドウのサイズで作成する
	ComPtr<ID3D12Resource> depthStencilResource = CreateDepthStencilTextureResource(
		device, // デバイス
		kClientWidth, // 幅
		kClientHeight); // 高さ

	// ディスクリプタヒープを生成する
	ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap = CreateDescriptorHeap(
		device, // デバイス
		D3D12_DESCRIPTOR_HEAP_TYPE_RTV, // レンダーターゲットビュー用
		2, // ダブルバッファ用に２つ
		false); // シェーダーからはアクセスしない

	// SRV用のディスクリプタヒープを生成する
	ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap = CreateDescriptorHeap(
		device, // デバイス
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, // SRV用
		128, // 128個用意する
		true); // シェーダーからアクセスする

	// DSV用のディスクリプタヒープ
	ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap = CreateDescriptorHeap(
		device, // デバイス
		D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
		1,
		false); // シェーダーからアクセスする

	// device 作成後に追加
	const uint32_t descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	const uint32_t descriptorSizeRTV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	const uint32_t descriptorSizeDSV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	// スワップチェーンからリソースを引っ張てくる
	ComPtr<ID3D12Resource> swapChainResources[2] = { nullptr };
	hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
	assert(SUCCEEDED(hr)); // スワップチェーンのリソース取得に失敗したらエラー
	hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
	assert(SUCCEEDED(hr)); // スワップチェーンのリソース取得に失敗したらエラー


	// RTVの設定
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // 色の形式
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	// ディスクリプタの先頭を取得する
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	// ディスクリプタを2つ用意
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2]{};
	// 1つめを作る
	rtvHandles[0] = rtvStartHandle;
	device->CreateRenderTargetView(swapChainResources[0].Get(), &rtvDesc, rtvHandles[0]);
	// 2つめを作る
	rtvHandles[1].ptr = rtvStartHandle.ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	device->CreateRenderTargetView(swapChainResources[1].Get(), &rtvDesc, rtvHandles[1]);

	// DSVの設定
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // 深度24bit、ステンシル8bit
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	// 先頭にDSVを作る
	device->CreateDepthStencilView(depthStencilResource.Get(), &dsvDesc,
		dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());


	// 初期値0でFenceを作る
	ComPtr<ID3D12Fence> fence = nullptr;
	uint64_t fenceValue = 0;
	hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&fence));
	assert(SUCCEEDED(hr)); // フェンスの生成に失敗したらエラー

	// FenceのSignalを待つためのイベントハンドルを作る
	HANDLE fenceEvent = CreateEvent(nullptr, false, false, nullptr);
	assert(fenceEvent != nullptr); // イベントハンドルの生成に失敗したらエラー

	// dxCompilerの初期化
	IDxcUtils* dxcUtils = nullptr;
	IDxcCompiler3* dxcCompiler = nullptr;
	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
	assert(SUCCEEDED(hr)); // dxcUtilsの生成に失敗したらエラー
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
	assert(SUCCEEDED(hr)); // dxcCompilerの生成に失敗したらエラー

	// includeに対応するための設定を行う
	IDxcIncludeHandler* includeHandler = nullptr;
	hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
	assert(SUCCEEDED(hr)); // includeHandlerの生成に失敗したらエラー

	// RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// RootParameter作成。PixelShaderのMaterialとVertexShaderのTransform 
	D3D12_ROOT_PARAMETER rootParameters[4] = {};

	// b0: MaterialCB (PixelShader)
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	// b1: TransformCB (VertexShader)
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].Descriptor.ShaderRegister = 1;

	// t0: SRVテクスチャ (PixelShader)
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

	// b3: DirectionalLight (PixelShader)
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].Descriptor.ShaderRegister = 3;

	// ルートシグネチャのセットアップ
	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);

	// Samplerの設定
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;   // 0~1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;   // 比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;   // ありったけのMipmapを使う
	staticSamplers[0].ShaderRegister = 0;   // レジスタ番号0を使う
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
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
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));

	// InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION"; // セマンティクス名
	inputElementDescs[0].SemanticIndex = 0; // セマンティクスのインデックス
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD"; // セマンティクス名
	inputElementDescs[1].SemanticIndex = 0; // セマンティクスのインデックス
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[2].SemanticName = "NORMAL"; // セマンティクス名
	inputElementDescs[2].SemanticIndex = 0; // セマンティクスのインデックス
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs; // セマンティクスの情報
	inputLayoutDesc.NumElements = _countof(inputElementDescs); // セマンティクスの数

	// BlendStateの設定
	D3D12_BLEND_DESC blendDesc{};
	// すべての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask =
		D3D12_COLOR_WRITE_ENABLE_ALL;

	// RasterizerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	// 裏面を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// Shaderのコンパイル
	IDxcBlob* vertexShaderBlob = CompileShader(
		L"Object3D.VS.hlsl", // コンパイルするファイルのパス
		L"vs_6_0", // プロファイル
		dxcUtils, // dxcUtils
		dxcCompiler, // dxcCompiler
		includeHandler); // includeHandler
	assert(vertexShaderBlob != nullptr); // シェーダーのコンパイルに失敗したらエラー

	IDxcBlob* pixelShaderBlob = CompileShader(
		L"Object3D.PS.hlsl", // コンパイルするファイルのパス
		L"ps_6_0", // プロファイル
		dxcUtils, // dxcUtils
		dxcCompiler, // dxcCompiler
		includeHandler); // includeHandler
	assert(pixelShaderBlob != nullptr); // シェーダーのコンパイルに失敗したらエラー

	// PSOを生成する
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature.Get(); // RootSignature
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc; // InputLayout
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() }; // VertexShader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() }; // PixelShader
	graphicsPipelineStateDesc.BlendState = blendDesc; // BlendState
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

	// 実際に生成
	ComPtr<ID3D12PipelineState> graphicsPipelineState = nullptr;
	hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));


	// リソース作成
	std::vector<ComPtr<ID3D12Resource>> textureResources;
	std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> textureSrvHandles;


	std::vector<GlassFragment> fragments = GenerateSphereFragmentsFromSingleModel(device, "glass_fragment.obj", 200);
	InitializeGlassFragmentBuffers(device, fragments);

	// 氷の地面
	ModelData iceBackgroundModel = LoadObjFile("Resources", "ice_background.obj");
	Transform iceTransform;
	iceTransform.scale = { 1.0f, 1.0f, 1.0f };
	iceTransform.rotate = { 0.0f, 0.0f, 0.0f };
	iceTransform.translate = { 0.0f, 0.0f, 0.0f }; // 球体の下に置くなら -5.0f にしてもOK
	Material iceMaterial{};
	iceMaterial.enableLighting = true;
	iceMaterial.color = { 0.6f, 0.9f, 1.1f, 0.15f }; // 淡い青＋透明



	// 頂点リソース用のヒープの設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; // アップロード用

	// A マテリアル（32バイト必要）
	ComPtr<ID3D12Resource> materialResourceA = CreateBufferResource(device, sizeof(Material));
	Material* materialDataA = nullptr;
	materialResourceA->Map(0, nullptr, reinterpret_cast<void**>(&materialDataA));
	*materialDataA = { {0.5f, 0.9f, 1.5f, 0.15f},1 }; // Lighting有効

	// A WVP（128バイト必要）
	ComPtr<ID3D12Resource> wvpResourceA = CreateBufferResource(device, sizeof(TransformationMatrix));
	TransformationMatrix* wvpDataA = nullptr;
	wvpResourceA->Map(0, nullptr, reinterpret_cast<void**>(&wvpDataA));
	wvpDataA->WVP = MakeIdentity4x4();
	wvpDataA->World = MakeIdentity4x4();


	// 平行光源用バッファ作成とマップ
	ComPtr<ID3D12Resource> directionalLightResource = CreateBufferResource(device, sizeof(DirectionalLight));
	DirectionalLight* directionalLightData = nullptr;
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));

	// データ設定
	directionalLightData->color = { 0.6f, 0.9f, 1.0f };
	Vector3 dir = Normalize({ 1.0f, -1.0f, 1.0f }); // 斜めにす
	directionalLightData->direction = { dir.x, dir.y, dir.z, 0.0f };

	directionalLightData->intensity = 1.0f;

	// ビューポート
	D3D12_VIEWPORT viewport{};
	// クライアント領域のサイズと一緒にして画面全体に表示
	viewport.Width = kClientWidth;
	viewport.Height = kClientHeight;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	// シザー矩形
	D3D12_RECT scissorRect{};
	// 基本的にビューポートと同じ矩形が構成されるようにする
	scissorRect.left = 0;
	scissorRect.right = kClientWidth;
	scissorRect.top = 0;
	scissorRect.bottom = kClientHeight;

	// Transform変数を作る
	static Transform transformA = {
		  {1.0f, 1.0f, 1.0f},  // scale
		  {0.0f, 0.0f, 0.0f},  // rotate
		  {0.0f, 0.0f, 0.0f}   // translate
	};

	// 現在選択されているテクスチャのインデックス
	static size_t selectedTextureIndex = 0;

	// Textureを呼んで転送する
	DirectX::ScratchImage mipImages = LoadTexture("resources/uvChecker.png");
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	ComPtr<ID3D12Resource> textureResource = CreateTextureResource(device, metadata);
	assert(textureResource);
	UploadTextureData(textureResource, mipImages);

	// metadataを基にSRVを作成する
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format; // フォーマット
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // コンポーネントマッピング
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels); // mipレベルの数

	//SRVを作成するDescriptorHeapの先頭を取得する
	UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 1);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 1);
	// テクスチャのSRVを作成する
	device->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);

	float deltaTime = 1.0f / 60.0f;        // フレーム時間（簡易固定値）

	bool isFiring = false;
	float fireTimer = 0.0f;
	float fireInterval = 0.05f; // 発射間隔（秒）
	std::queue<GlassFragment*> fireQueue;

	std::mt19937 rng(std::random_device{}());



	// キーの状態
	static BYTE key[256] = {};
	static BYTE keyPre[256] = {};

	LPDIRECTINPUT8 directInput = nullptr;
	LPDIRECTINPUTDEVICE8 keyboard = nullptr;

	hr = DirectInput8Create(
		GetModuleHandle(nullptr),   // インスタンスハンドル
		DIRECTINPUT_VERSION,        // DirectInputのバージョン
		IID_IDirectInput8,          // インターフェースID
		(void**)&directInput,       // 取得されるオブジェクト
		nullptr                     // 外部コンテナ（使わないならnullptr）
	);
	assert(SUCCEEDED(hr)); // 失敗してたらクラッシュさせて検出


	// キーボードデバイスの作成
	hr = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, nullptr);
	assert(SUCCEEDED(hr));

	// データフォーマットの設定
	hr = keyboard->SetDataFormat(&c_dfDIKeyboard);
	assert(SUCCEEDED(hr));

	// 協調レベルの設定
	hr = keyboard->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	assert(SUCCEEDED(hr));

	// 入力開始
	keyboard->Acquire();
	keyboard->GetDeviceState(sizeof(key), key);

	// デバッグカメラの初期化
	DebugCamera debugCamera;
	bool useDebugCamera = false;

	debugCamera.Initialize();

	// Imguiの初期化
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX12_Init(device.Get(), swapChainDesc.BufferCount,
		rtvDesc.Format,
		srvDescriptorHeap.Get(), // SRV用のヒープ
		srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), // CPU側のヒープ
		srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart()); // GPU側のヒープ

	MSG msg{};
	// ウィンドウのxボタンが押されるまでループ
	while (msg.message != WM_QUIT) {
		// Windowsにメッセージが来てたら最優先で処理させる
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			// メッセージがあったら処理する
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else {
			// ゲームの処理
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			// 毎フレームのキーボード入力取得
			BYTE currentKey[256];
			memcpy(keyPre, key, sizeof(key));
			memcpy(key, currentKey, sizeof(key));
			keyboard->GetDeviceState(sizeof(currentKey), currentKey);

			// デバッグテキストの表示
			ImGui::Begin("Window");
		
			// 光の方向ベクトルの編集
			static Vector3 lightDirEdit = { directionalLightData->direction.x, directionalLightData->direction.y, directionalLightData->direction.z };
			if (ImGui::DragFloat3("Light Dir", &lightDirEdit.x, 0.01f, -1.0f, 1.0f)) {
				// 正規化して反映
				Vector3 normDir = Normalize(lightDirEdit);
				directionalLightData->direction = { normDir.x, normDir.y, normDir.z, 0.0f };
			}
			// 光の強さ
			ImGui::DragFloat("Light Intensity", &directionalLightData->intensity, 0.01f, 0.0f, 10.0f);
			ImGui::ColorEdit3("Light Color", &directionalLightData->color.x);
			// ImGui内のWindow内に追加（確認用）
			ImGui::Checkbox("Use Debug Camera", &useDebugCamera);

			if (ImGui::Button("Trigger Break")) {
				std::shuffle(fragments.begin(), fragments.end(), std::mt19937{ std::random_device{}() });
				fireQueue = std::queue<GlassFragment*>();
				for (auto& frag : fragments) {
					frag.isFlying = false;
					frag.velocity = { 0, 0, 0 };
					fireQueue.push(&frag);
				}
				isFiring = true;
				fireTimer = 0.0f;
			}

			static Vector3 globalOffset = { 0.0f, 0.0f, 0.0f };
			ImGui::DragFloat3("Global Offset", &globalOffset.x, 0.01f);

			if (ImGui::Button("Apply Offset")) {
				for (auto& frag : fragments) {
					frag.transform.translate += globalOffset;
				}
				globalOffset = { 0.0f, 0.0f, 0.0f }; // 加算後リセット（任意）
			}
			ImGui::End();



			// DebugCamera更新
			if (useDebugCamera) {
				debugCamera.Update(currentKey);
			}

			// View / Projection 行列
			Matrix4x4 viewMatrix;
			Matrix4x4 projectionMatrix;

			if (useDebugCamera) {
				viewMatrix = debugCamera.GetViewMatrix();
				projectionMatrix = debugCamera.GetProjectionMatrix();
			} else {
				// 固定カメラ
				viewMatrix = MakeLookLhMatrix(
					{ 0.0f, 0.0f, -5.0f },
					{ 0.0f, 0.0f,  0.0f },
					{ 0.0f, 1.0f,  0.0f }
				);
				projectionMatrix = MakePerspectiveFovMatrix(
					0.45f, 1280.0f / 720.0f, 0.1f, 100.0f
				);
			}
			if (isFiring) {
				fireTimer += deltaTime;
				while (!fireQueue.empty() && fireTimer >= fireInterval) {
					GlassFragment* frag = fireQueue.front();
					fireQueue.pop();

					frag->isFlying = true;
					frag->velocity = { 0, 0, 8.0f }; // Z方向（手前）にビームのように飛ぶ
					fireTimer -= fireInterval;
				}

				if (fireQueue.empty()) {
					isFiring = false;
				}
			}

			// 各破片の位置を更新
			for (auto& frag : fragments) {
				if (frag.isFlying) {
					frag.transform.translate += frag.velocity * deltaTime;
				}
			}




			// ImGuiの描画
			ImGui::Render();

			// バックバッファのインデックスを取得
			UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();
			// TransitionBarrierの設定
			D3D12_RESOURCE_BARRIER barrier{};
			// 今回のバリアはTransition
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			// Noneにしておく
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			// バリアを張る対象のリソース。現在のバックバッファに対して行う
			barrier.Transition.pResource = swapChainResources[backBufferIndex].Get();
			// 遷移前のResourceState
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			// 遷移後のResourceState
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			// TransitionBarrierを張る
			commandList->ResourceBarrier(1, &barrier);

			// 描画先のRTVを設定
			commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, nullptr);

			// 指定した色で画面全体をクリア
			float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
			commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);

			// ビューポートとシザーの設定
			commandList->RSSetViewports(1, &viewport);
			commandList->RSSetScissorRects(1, &scissorRect);

			// デスクリプタヒープの設定
			ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap.Get() };
			commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

			// 深度バッファのクリア
			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);
			commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);


			// RootSignatureとPSOの設定
			commandList->SetGraphicsRootSignature(rootSignature.Get());
			commandList->SetPipelineState(graphicsPipelineState.Get());

			// 頂点バッファの設定
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);



			// 描画処理
			UpdateAndDrawFragments(
				commandList.Get(),
				fragments,
				materialResourceA,
				directionalLightResource,
				textureSrvHandleGPU,
				viewMatrix,
				projectionMatrix
			);




			// ImGuiの描画
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());

			// RenderTargetからPresentにする
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			// TransitionBarrierを張る
			commandList->ResourceBarrier(1, &barrier);
			// コマンドリストを確定させてクローズ
			hr = commandList->Close();
			assert(SUCCEEDED(hr)); // コマンドリストのクローズに失敗したらエラー


			// GPUにコマンドリストを実行させる
			ID3D12CommandList* commandLists[] = { commandList.Get() };
			commandQueue->ExecuteCommandLists(1, commandLists);
			// GPUとOSに画面の交換をさせる
			swapChain->Present(1, 0);
			// Fenceの値を更新
			fenceValue++;
			// GPUがここまできたとき、Fenceの値を指定した値に代入するようにSignalを送る
			commandQueue->Signal(fence.Get(), fenceValue);
			// Fenceの値が指定した値になったか確認する
			if (fence->GetCompletedValue() < fenceValue) {
				// 指定した値になっていなかったら、指定した値になるまで待つように設定する
				fence->SetEventOnCompletion(fenceValue, fenceEvent);
				// 指定した値になるまで待つ
				WaitForSingleObject(fenceEvent, INFINITE);
			}

			// 次のフレーム用のコマンドリストを取得
			hr = commandAllocator->Reset();
			assert(SUCCEEDED(hr)); // コマンドアロケータのリセットに失敗したらエラー
			// コマンドリストをリセット
			hr = commandList->Reset(commandAllocator.Get(), nullptr);
			assert(SUCCEEDED(hr)); // コマンドリストのリセットに失敗したらエラー
		}
	}

	if (keyboard) {
		keyboard->Unacquire();
		keyboard->Release();
		keyboard = nullptr;
	}
	if (directInput) {
		directInput->Release();
		directInput = nullptr;
	}


	// 出力ウィンドウへの文字出力
	OutputDebugStringA("Hello, DirectX!\n");

	// 解放処理
	CloseHandle(fenceEvent);

	CloseWindow(hwnd);

	// ImGuiの終了処理
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	// COMの終了処理
	CoUninitialize();
	return 0;
}