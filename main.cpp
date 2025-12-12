#include <Windows.h>
#include <cstdint>
#include <vector>
#include <cmath>
#include <string>
#include <format>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <dxcapi.h>
#include <cassert>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <wrl.h>
#include "ResourceObject.h"
#include <wrl/client.h>
#include <xaudio2.h>
#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定
#include <dinput.h>
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "externals/DirectXTex/DirectXTex.h"
#include <random>
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

using namespace Microsoft::WRL;

// ベクター2
struct Vector2 {
	float x, y;
};

// ベクター3
struct Vector3 {
	float x, y, z;
};

// ベクター4
struct Vector4 {
	float x, y, z, w;
};

// 4x4行列の定義
struct Matrix4x4 {
	float m[4][4];
};

struct Transform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

struct VertexData {
	Vector4 position; // 頂点の位置
	Vector2 texcoord; // テクスチャ座標
	Vector3 normal;   // 法線ベクトル
};

struct Material {
	Vector4 color;
	int32_t lightingMode;
	float padding[3]; // 16バイトアライメント維持
	Matrix4x4 uvTransform;
	std::string textureFilePath; // ← これを追加！
};


struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
};

struct DirectionalLight {
	Vector4 color;
	Vector4 direction;
	float intensity;
	Vector3 padding; // ← float3 paddingで16バイト境界に揃える
};



struct MaterialData {
	std::string textureFilePath;
};

struct ModelData {
	std::vector<VertexData> vertices; // 頂点データ
	MaterialData material; // マテリアルデータ
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

// RIFFヘッダチャンク
struct RiffHeader {
	ChunkHeader chunk; // "RIFF"
	char type[4];      // "WAVE"
};

// FMTチャンク
struct FormatChunk {
	ChunkHeader chunk; // "fmt "
	WAVEFORMATEX fmt;  // 波形フォーマット
};

// 音声データ
struct SoundData {
	// 波形フォーマット
	WAVEFORMATEX wfex; // 波形フォーマット
	// バッファの先頭アドレス
	BYTE* pBuffer; // 音声データのポインタ
	// バッファのサイズ
	unsigned int bufferSize; // 音声データのサイズ
};

// モデル選択用
enum class ModelType {
	Plane,
	Sphere,
	UtahTeapot,
	StanfordBunny,
	MultiMesh,
	MultiMaterial,
	Fence,
	InstancingPlane
};

enum class LightingMode {
	None,
	Lambert,
	HalfLambert,
};

struct Mesh {
	std::vector<VertexData> vertices;
	std::string name;
	std::string materialName;
};

struct MultiModelData {
	std::vector<Mesh> meshes;
	std::unordered_map<std::string, Material> materials;
};

struct MeshRenderData {
	ComPtr<ID3D12Resource> vertexResource;
	D3D12_VERTEX_BUFFER_VIEW vbView;
	size_t vertexCount;
	std::string name;
	std::string materialName;
};
MultiModelData multiModel;
std::vector<MeshRenderData> meshRenderList;

enum class BlendMode {
	None,       // ブレンドなし
	Normal,     // Src*SrcA + Dst*(1-SrcA)
	Add,        // Src*SrcA + Dst
	Subtract,   // Dst - Src*SrcA
	Multiply,   // Src*Dst
	Screen,     // Src*(1-Dst) + Dst
	Count,
};

struct InstanceData {
	Matrix4x4 world;
	Vector4 color;   // ← 追加
};

struct Particle {
	Vector3 position;
	Vector3 velocity;
	Vector3 rotation;
	Vector4 color;
	float life;
	float currentLife;

};


Particle particles[10];
std::random_device rd;
std::mt19937 randomEngine(rd());


// 単位行列の作成
Matrix4x4 MakeIdentity4x4() {
	Matrix4x4 result;
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			if (i == j) {
				result.m[i][j] = 1.0f;
			} else {
				result.m[i][j] = 0.0f;
			}
		}
	}
	return result;
}

// 4x4行列の積
Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2) {
	Matrix4x4 result;
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			result.m[i][j] = 0;
			for (int k = 0; k < 4; k++) {
				result.m[i][j] += m1.m[i][k] * m2.m[k][j];
			}
		}
	}
	return result;
}

// 拡大縮小
Matrix4x4 MakeScaleMatrix(const Vector3& scale) {
	Matrix4x4 result{};
	result.m[0][0] = scale.x;
	result.m[1][1] = scale.y;
	result.m[2][2] = scale.z;
	result.m[3][3] = 1.0f;
	return result;
}

Matrix4x4 MakeTranslateMatrix(const Vector3& translate) {
	Matrix4x4 result{};
	result.m[0][0] = 1.0f;
	result.m[1][1] = 1.0f;
	result.m[2][2] = 1.0f;
	result.m[3][3] = 1.0f;
	result.m[3][0] = translate.x;
	result.m[3][1] = translate.y;
	result.m[3][2] = translate.z;
	return result;
}


// X軸回転行列
Matrix4x4 MakeRotateXMatrix(float angle) {
	Matrix4x4 result = {};
	result.m[0][0] = 1.0f;
	result.m[3][3] = 1.0f;
	result.m[1][1] = std::cos(angle);
	result.m[1][2] = std::sin(angle);
	result.m[2][1] = -std::sin(angle);
	result.m[2][2] = std::cos(angle);
	return result;
}
// Y軸回転行列
Matrix4x4 MakeRotateYMatrix(float angle) {
	Matrix4x4 result = {};
	result.m[1][1] = 1.0f;
	result.m[3][3] = 1.0f;
	result.m[0][0] = std::cos(angle);
	result.m[0][2] = -std::sin(angle);
	result.m[2][0] = std::sin(angle);
	result.m[2][2] = std::cos(angle);
	return result;
}
// Z軸回転行列
Matrix4x4 MakeRotateZMatrix(float angle) {
	Matrix4x4 result = {};
	result.m[2][2] = 1.0f;
	result.m[3][3] = 1.0f;
	result.m[0][0] = std::cos(angle);
	result.m[0][1] = std::sin(angle);
	result.m[1][0] = -std::sin(angle);
	result.m[1][1] = std::cos(angle);
	return result;
}

// アフィン変換行列
Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate) {
	Matrix4x4 result = {};
	// X,Y,Z軸の回転をまとめる
	Matrix4x4 rotateXYZ =
		Multiply(MakeRotateXMatrix(rotate.x), Multiply(MakeRotateYMatrix(rotate.y), MakeRotateZMatrix(rotate.z)));

	result.m[0][0] = scale.x * rotateXYZ.m[0][0];
	result.m[0][1] = scale.x * rotateXYZ.m[0][1];
	result.m[0][2] = scale.x * rotateXYZ.m[0][2];
	result.m[1][0] = scale.y * rotateXYZ.m[1][0];
	result.m[1][1] = scale.y * rotateXYZ.m[1][1];
	result.m[1][2] = scale.y * rotateXYZ.m[1][2];
	result.m[2][0] = scale.z * rotateXYZ.m[2][0];
	result.m[2][1] = scale.z * rotateXYZ.m[2][1];
	result.m[2][2] = scale.z * rotateXYZ.m[2][2];
	result.m[3][0] = translate.x;
	result.m[3][1] = translate.y;
	result.m[3][2] = translate.z;
	result.m[3][3] = 1.0f;

	return result;
}

// 3x3の行列式を計算
static float Determinant3x3(float matrix[3][3]) {
	return matrix[0][0] * (matrix[1][1] * matrix[2][2] - matrix[1][2] * matrix[2][1]) -
		matrix[0][1] * (matrix[1][0] * matrix[2][2] - matrix[1][2] * matrix[2][0]) +
		matrix[0][2] * (matrix[1][0] * matrix[2][1] - matrix[1][1] * matrix[2][0]);
}

// 4x4行列の余因子を計算
static float Minor(const Matrix4x4& m, int row, int col) {
	float sub[3][3];
	int sub_i = 0;
	for (int i = 0; i < 4; ++i) {
		if (i == row) continue;
		int sub_j = 0;
		for (int j = 0; j < 4; ++j) {
			if (j == col) continue;
			sub[sub_i][sub_j] = m.m[i][j];
			sub_j++;
		}
		sub_i++;
	}

	// 3x3行列の行列式を計算
	return Determinant3x3(sub);
}

// 4x4行列の逆行列を計算
static Matrix4x4 Inverse(const Matrix4x4& m) {
	Matrix4x4 result = {};

	// 4x4行列の行列式を計算
	float det = 0.0f;
	for (int col = 0; col < 4; ++col) {
		int sign = (col % 2 == 0) ? 1 : -1;
		det += sign * m.m[0][col] * Minor(m, 0, col);
	}

	// 行列式が0の場合は逆行列が存在しない
	if (det == 0.0f) {
		return result;
	}

	float invDet = 1.0f / det;

	// 各要素の計算
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			int sign = ((i + j) % 2 == 0) ? 1 : -1;
			result.m[j][i] = sign * Minor(m, i, j) * invDet;
		}
	}

	return result;
}

// 透視投影行列
Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip) {
	Matrix4x4 result = {};
	result.m[0][0] = 1.0f / (aspectRatio * std::tan(fovY / 2.0f));
	result.m[1][1] = 1.0f / std::tan(fovY / 2.0f);
	result.m[2][2] = farClip / (farClip - nearClip);
	result.m[2][3] = 1.0f;
	result.m[3][2] = -(farClip * nearClip) / (farClip - nearClip);
	return result;
}

// 平行投影行列（左手座標系）
Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) {
	Matrix4x4 result = {};

	result.m[0][0] = 2.0f / (right - left);
	result.m[1][1] = 2.0f / (top - bottom);
	result.m[2][2] = 1.0f / (farClip - nearClip);
	result.m[3][0] = (left + right) / (left - right);
	result.m[3][1] = (top + bottom) / (bottom - top);
	result.m[3][2] = -nearClip / (farClip - nearClip);
	result.m[3][3] = 1.0f;

	return result;
}

Vector3 operator*(const Vector3& v, float s) {
	return { v.x * s, v.y * s, v.z * s };
}

Vector3 operator+(const Vector3& a, const Vector3& b) {
	return { a.x + b.x, a.y + b.y, a.z + b.z };
}

Vector3& operator+=(Vector3& a, const Vector3& b) {
	a.x += b.x;
	a.y += b.y;
	a.z += b.z;
	return a;
}


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

// 球メッシュ生成
void GenerateSphereMesh(std::vector<VertexData>& outVertices, std::vector<uint32_t>& outIndices, int latitudeCount, int longitudeCount) {
	const float radius = 1.0f;
	for (int lat = 0; lat <= latitudeCount; ++lat) {
		float theta = lat * DirectX::XM_PI / latitudeCount;
		float sinTheta = std::sin(theta);
		float cosTheta = std::cos(theta);

		for (int lon = 0; lon <= longitudeCount; ++lon) {
			float phi = lon * 2.0f * DirectX::XM_PI / longitudeCount;
			float sinPhi = std::sin(phi);
			float cosPhi = std::cos(phi);

			Vector3 pos = {
				radius * sinTheta * cosPhi,
				radius * cosTheta,
				radius * sinTheta * sinPhi
			};
			Vector2 uv = {
				float(lon) / longitudeCount,
				float(lat) / latitudeCount
			};
			// 法線は位置を正規化して使う（球の中心から放射状）
			Vector3 normal = pos;
			float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
			if (length != 0.0f) {
				normal.x /= length;
				normal.y /= length;
				normal.z /= length;
			}
			outVertices.push_back({ {pos.x, pos.y, pos.z, 1.0f}, uv, normal });

		}
	}

	for (int lat = 0; lat < latitudeCount; ++lat) {
		for (int lon = 0; lon < longitudeCount; ++lon) {
			int current = lat * (longitudeCount + 1) + lon;
			int next = current + longitudeCount + 1;
			// 反時計回りに修正
			outIndices.push_back(current + 1);
			outIndices.push_back(next);
			outIndices.push_back(current);

			outIndices.push_back(next + 1);
			outIndices.push_back(next);
			outIndices.push_back(current + 1);

		}
	}
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

Vector3 Normalize(const Vector3& v) {
	float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	if (length == 0.0f) return { 0.0f, 0.0f, 0.0f };
	return { v.x / length, v.y / length, v.z / length };
}

void SetVertex(VertexData& v, const Vector4& pos, const Vector2& uv) {
	v.position = pos;
	v.texcoord = uv;
	Vector3 p = { pos.x, pos.y, pos.z };
	v.normal = Normalize(p);
}

MaterialData LoadMaterialTemplate(const std::string& directoryPath, const std::string& filename) {
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

std::unordered_map<std::string, Material> LoadMaterialTemplateMulti(
	const std::string& directoryPath,
	const std::string& filename)
{
	std::unordered_map<std::string, Material> materials;
	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open());

	std::string line;
	std::string currentMaterialName;
	Material currentMaterial{};

	while (std::getline(file, line)) {
		std::istringstream s(line);
		std::string identifier;
		s >> identifier;

		if (identifier == "newmtl") {
			// 直前のマテリアルを保存
			if (!currentMaterialName.empty()) {
				materials[currentMaterialName] = currentMaterial;
			}

			// 新しいマテリアル名
			s >> currentMaterialName;
			currentMaterial = Material(); // 初期化
			currentMaterial.color = { 1.0f, 1.0f, 1.0f, 1.0f };
			currentMaterial.lightingMode = 1; // Lambertなど
			currentMaterial.uvTransform = MakeIdentity4x4();
		} else if (identifier == "Kd") {
			// 拡散反射色
			s >> currentMaterial.color.x >> currentMaterial.color.y >> currentMaterial.color.z;
			currentMaterial.color.w = 1.0f;
		} else if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;
			currentMaterial.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}

	// 最後のマテリアルを保存
	if (!currentMaterialName.empty()) {
		materials[currentMaterialName] = currentMaterial;
	}

	return materials;
}



ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename) {
	ModelData modelData;
	std::vector<Vector4> positions;  // 頂点位置
	std::vector<Vector2> texcoords; // テクスチャ座標
	std::vector<Vector3> normals; // 法線ベクトル
	std::string line; // ファイルから読んだ1行を格納するもの

	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open()); // ファイルが開けなかったらエラー

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);

		s >> identifier; // 行の先頭の文字列を取得
		if (identifier == "v") { // 頂点位置
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.w = 1.0f; // 同次座標系のためw成分を1に設定
			positions.push_back(position);
		} else if (identifier == "vt") { // テクスチャ座標
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoords.push_back(texcoord);
		} else if (identifier == "vn") { // 法線ベクトル
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normals.push_back(normal);
		} else if (identifier == "f") { // 面情報
			VertexData triangle[3];
			// 面は三角形限定。他のは未対応
			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
				std::string vertexDefinition;
				s >> vertexDefinition;

				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3];
				for (int32_t element = 0; element < 3; ++element) {
					std::string index;
					std::getline(v, index, '/');
					elementIndices[element] = std::stoi(index);
				}

				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcoord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];

				// 🔁 座標系変換：X軸反転（右手 → 左手）
				// position.x *= -1.0f; ← やらない
				// normal.x *= -1.0f; ← やらない
				texcoord.y = 1.0f - texcoord.y;


				triangle[faceVertex] = { position, texcoord, normal };
			}

			// 🔁 頂点の登録順を逆順にする（面の回り順を逆にする）
			modelData.vertices.push_back(triangle[2]);
			modelData.vertices.push_back(triangle[1]);
			modelData.vertices.push_back(triangle[0]);
		} else if (identifier == "mtllib") {
			std::string materialFilename;
			s >> materialFilename;
			modelData.material = LoadMaterialTemplate(directoryPath, materialFilename);
		}
	}
	return modelData;
}

MultiModelData LoadObjFileMulti(const std::string& directoryPath, const std::string& filename) {
	MultiModelData modelData;

	std::vector<Vector4> positions;
	std::vector<Vector2> texcoords;
	std::vector<Vector3> normals;

	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open());

	std::string line;
	std::string currentMeshName = "default";
	std::string currentMaterialName = "default"; // 現在のマテリアル名
	Mesh currentMesh;

	while (std::getline(file, line)) {
		std::istringstream s(line);
		std::string identifier;
		s >> identifier;

		if (identifier == "v") {
			Vector4 pos; s >> pos.x >> pos.y >> pos.z;
			pos.z *= -1.0f;
			pos.w = 1.0f;
			positions.push_back(pos);
		} else if (identifier == "vt") {
			Vector2 uv; s >> uv.x >> uv.y;
			texcoords.push_back(uv);
		} else if (identifier == "vn") {
			Vector3 n; s >> n.x >> n.y >> n.z;
			n.z *= -1.0f;
			normals.push_back(n);
		} else if (identifier == "f") {
			VertexData tri[3];
			for (int i = 0; i < 3; ++i) {
				std::string vtx;
				s >> vtx;
				std::istringstream vs(vtx);
				uint32_t idx[3] = {};
				for (int j = 0; j < 3; ++j) {
					std::string val;
					std::getline(vs, val, '/');
					idx[j] = std::stoi(val);
				}
				tri[i] = {
					positions[idx[0] - 1],
					{ texcoords[idx[1] - 1].x, 1.0f - texcoords[idx[1] - 1].y },
					normals[idx[2] - 1]
				};
			}
			currentMesh.vertices.push_back(tri[2]);
			currentMesh.vertices.push_back(tri[1]);
			currentMesh.vertices.push_back(tri[0]);
		} else if (identifier == "g" || identifier == "o") {
			if (!currentMesh.vertices.empty()) {
				currentMesh.name = currentMeshName;
				currentMesh.materialName = currentMaterialName; // 使用中のマテリアル名を記録
				modelData.meshes.push_back(currentMesh);
				currentMesh = Mesh(); // 次のMeshへ
			}
			s >> currentMeshName;
		} else if (identifier == "mtllib") {
			std::string mtl;
			s >> mtl;
			modelData.materials = LoadMaterialTemplateMulti(directoryPath, mtl); // マテリアル複数対応版
		} else if (identifier == "usemtl") {
			// 現在のマテリアル名を更新
			s >> currentMaterialName;

			// もし現メッシュに頂点があれば、いったん保存してマテリアル名を更新
			if (!currentMesh.vertices.empty()) {
				currentMesh.name = currentMeshName;
				currentMesh.materialName = currentMaterialName;
				modelData.meshes.push_back(currentMesh);
				currentMesh = Mesh(); // 次のメッシュへ切り替え
			}
		}

	}

	if (!currentMesh.vertices.empty()) {
		currentMesh.name = currentMeshName;
		currentMesh.materialName = currentMaterialName;
		modelData.meshes.push_back(currentMesh);
	}

	return modelData;
}


// 音声データの読み込み
SoundData SoundLoadWave(const char* filename) {
	// HRESULT result;
	// ファイル入力ストリームのインスタンス
	std::ifstream file;
	// .wavファイルをバイナリモードで開く
	file.open(filename, std::ios_base::binary);
	// ファイルオープン失敗を検出する
	assert(file.is_open());

	// RIFFヘッダーの読み込み
	RiffHeader riff;
	file.read((char*)&riff, sizeof(riff));

	// ファイルがRIFFかチェック
	if (strncmp(riff.chunk.id, "RIFF", 4) != 0) {
		assert(0);
	}
	// タイプがWAVEかチェック
	if (strncmp(riff.type, "WAVE", 4) != 0) {
		assert(0);
	}

	// Formatチャンクの読み込み
	FormatChunk format = {};
	// チャンクヘッダーの確認
	file.read((char*)&format, sizeof(ChunkHeader));
	if (strncmp(format.chunk.id, "fmt ", 4) != 0) {
		assert(0);
	}

	// チャンク本体の読み込み
	assert(format.chunk.size <= sizeof(format.fmt));
	file.read((char*)&format.fmt, format.chunk.size);

	// Dataチャンクの読み込み
	ChunkHeader data;
	file.read((char*)&data, sizeof(data));

	// JUNKチャンクを検出した場合
	if (strncmp(data.id, "JUNK", 4) == 0) {
		// 読み取り位置をJUNKチャンクの終わりまで進める
		file.seekg(data.size, std::ios_base::cur);
		// 再読み込み
		file.read((char*)&data, sizeof(data));
	}

	if (strncmp(data.id, "data", 4) != 0) {
		assert(0);
	}

	// Dataチャンクのデータ部（波形データ）の読み込み
	char* pBuffer = new char[data.size];
	file.read(pBuffer, data.size);

	// Waveファイルを閉じる
	file.close();

	// returnする為の音声データ
	SoundData soundData = {};

	soundData.wfex = format.fmt;
	soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
	soundData.bufferSize = data.size;

	return soundData;
}

// 音声データ解放
void SoundUnload(SoundData* soundData)
{
	// バッファのメモリを解放
	delete[] soundData->pBuffer;

	soundData->pBuffer = 0;
	soundData->bufferSize = 0;
	soundData->wfex = {};
}

// 音声再生
void SoundPlayWave(IXAudio2* xAudio2, const SoundData& soundData) {
	HRESULT result;

	// 波形フォーマットを元に SourceVoice の生成
	IXAudio2SourceVoice* pSourceVoice = nullptr;
	result = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
	assert(SUCCEEDED(result));

	// 再生する波形データの設定
	XAUDIO2_BUFFER buf{};
	buf.pAudioData = soundData.pBuffer;
	buf.AudioBytes = soundData.bufferSize;
	buf.Flags = XAUDIO2_END_OF_STREAM;

	// 波形データの再生
	result = pSourceVoice->SubmitSourceBuffer(&buf);
	result = pSourceVoice->Start();
}

// モデルのファイル名を取得する関数
const char* GetModelFileName(ModelType type) {
	switch (type) {
	case ModelType::Plane: return "plane.obj";
	case ModelType::UtahTeapot: return "teapot.obj";
	case ModelType::StanfordBunny: return "bunny.obj";
	case ModelType::MultiMesh: return "multiMesh.obj";
	case ModelType::MultiMaterial: return "multiMaterial.obj";
	case ModelType::Fence: return "fence.obj";
	default: return "plane.obj";
	}
}

auto NormalizeTextureKey = [](const std::string& path) -> std::string {
	std::string filename = std::filesystem::path(path).filename().string();
	std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
	return filename;
	};

LPDIRECTINPUT8 directInput = nullptr;
LPDIRECTINPUTDEVICE8 gamepad = nullptr;

void InitGamepad(HWND hwnd) {
	// DirectInputオブジェクトの生成
	HRESULT hr = DirectInput8Create(
		GetModuleHandle(nullptr),
		DIRECTINPUT_VERSION,
		IID_IDirectInput8,
		(void**)&directInput,
		nullptr);
	assert(SUCCEEDED(hr));

	// ゲームパッドの列挙と取得
	directInput->EnumDevices(DI8DEVCLASS_GAMECTRL,
		[](const DIDEVICEINSTANCE* pdidInstance, VOID* pContext) -> BOOL {
			HRESULT hr = directInput->CreateDevice(pdidInstance->guidInstance, &gamepad, nullptr);
			if (FAILED(hr)) return DIENUM_CONTINUE;

			gamepad->SetDataFormat(&c_dfDIJoystick);
			gamepad->SetCooperativeLevel((HWND)pContext, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
			gamepad->Acquire();
			return DIENUM_STOP; // 最初のゲームパッドで止める
		}, hwnd, DIEDFL_ATTACHEDONLY);
}

static D3D12_BLEND_DESC MakeBlendDesc(BlendMode mode) {
	D3D12_BLEND_DESC desc{};
	auto& rt = desc.RenderTarget[0];
	rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	rt.BlendEnable = TRUE;
	rt.BlendOp = D3D12_BLEND_OP_ADD;
	rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;

	switch (mode) {
	case BlendMode::None:
		rt.BlendEnable = FALSE;               // 書き込みだけ
		rt.SrcBlend = D3D12_BLEND_ONE;
		rt.DestBlend = D3D12_BLEND_ZERO;
		rt.SrcBlendAlpha = D3D12_BLEND_ONE;
		rt.DestBlendAlpha = D3D12_BLEND_ZERO;
		break;
	case BlendMode::Normal:                      // Result = S*Sa + D*(1-Sa)
		rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		rt.SrcBlendAlpha = D3D12_BLEND_ONE;
		rt.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA; // A = Sa + Da*(1-Sa)
		break;
	case BlendMode::Add:                         // Result = S*Sa + D
		rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		rt.DestBlend = D3D12_BLEND_ONE;
		rt.SrcBlendAlpha = D3D12_BLEND_ONE;
		rt.DestBlendAlpha = D3D12_BLEND_ONE;
		break;
	case BlendMode::Subtract:                    // Result = D - S*Sa
		rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		rt.DestBlend = D3D12_BLEND_ONE;
		rt.BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
		rt.SrcBlendAlpha = D3D12_BLEND_ONE;
		rt.DestBlendAlpha = D3D12_BLEND_ONE;
		rt.BlendOpAlpha = D3D12_BLEND_OP_REV_SUBTRACT;
		break;
	case BlendMode::Multiply:                    // Result = S*D
		rt.SrcBlend = D3D12_BLEND_DEST_COLOR;
		rt.DestBlend = D3D12_BLEND_ZERO;
		rt.SrcBlendAlpha = D3D12_BLEND_DEST_ALPHA;
		rt.DestBlendAlpha = D3D12_BLEND_ZERO;
		break;
	case BlendMode::Screen:                      // Result = S*(1-D) + D
		rt.SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
		rt.DestBlend = D3D12_BLEND_ONE;
		rt.SrcBlendAlpha = D3D12_BLEND_ONE;
		rt.DestBlendAlpha = D3D12_BLEND_INV_DEST_ALPHA;
		break;
	}
	return desc;
}

Particle MakeNewParticle(std::mt19937& randomEngine)
{
	Particle p{};
	std::uniform_real_distribution<float> distVel(-1.0f, 1.0f);
	std::uniform_real_distribution<float> distCol(0.0f, 1.0f);

	p.position = { 0.0f, 0.0f, 0.0f };
	p.velocity = { distVel(randomEngine), distVel(randomEngine), 0.0f };

	p.rotation = { 0.0f, 0.0f, 0.0f };   // ★追加

	p.color = { distCol(randomEngine), distCol(randomEngine), distCol(randomEngine), 1.0f };

	p.life = 2.0f;
	p.currentLife = 0.0f;               // ★追加
	return p;
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
		L"LE2B_06_コイズミ_リョウ", // ウィンドウ名
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
	D3D12_INPUT_ELEMENT_DESC inputElements[] = {
		// Stream0（頂点データ）
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,
		  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16,
		  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24,
		  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

		  // Stream1（インスタンス行列）
		  { "INSTANCE", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0,
			D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },

		  { "INSTANCE", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16,
			D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },

		  { "INSTANCE", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32,
			D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },

		  { "INSTANCE", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48,
			D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
	};

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElements;
	inputLayoutDesc.NumElements = _countof(inputElements);


	// RasterizerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	// 裏面を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
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

	ComPtr<IXAudio2> xAudio2 = nullptr;
	HRESULT result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	assert(SUCCEEDED(result));

	IXAudio2MasteringVoice* masterVoice = nullptr;
	result = xAudio2->CreateMasteringVoice(&masterVoice);
	assert(SUCCEEDED(result));

	// DepthStencilの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// どこか適切なスコープで保持
	ComPtr<ID3D12PipelineState> pipelineByBlend[(int)BlendMode::Count];

	// …シェーダやRS等を入れた graphicsPipelineStateDesc が出来たあとで：
	for (int i = 0; i < (int)BlendMode::Count; ++i) {
		graphicsPipelineStateDesc.BlendState = MakeBlendDesc((BlendMode)i);
		hr = device->CreateGraphicsPipelineState(
			&graphicsPipelineStateDesc, IID_PPV_ARGS(&pipelineByBlend[i]));
		assert(SUCCEEDED(hr));
	}

	// DirectInputの初期化
	IDirectInput8* directInput = nullptr;
	result = DirectInput8Create(
		GetModuleHandle(nullptr), // ← これで現在のインスタンスハンドルを取得
		DIRECTINPUT_VERSION, IID_IDirectInput8,
		(void**)&directInput, nullptr);
	assert(SUCCEEDED(result));

	// キーボードデバイスの生成
	IDirectInputDevice8* keyboard = nullptr;
	result = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
	assert(SUCCEEDED(result));

	// 入力データ形式のセット
	result = keyboard->SetDataFormat(&c_dfDIKeyboard); // 標準形式
	assert(SUCCEEDED(result));

	// 排他制御レベルのセット
	result = keyboard->SetCooperativeLevel(
		hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(result));


	// 実際に生成
	ComPtr<ID3D12PipelineState> graphicsPipelineState = nullptr;
	hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));

	InitGamepad(hwnd); // ゲームパッドを初期化


	// モデルデータの読み込み
	ModelData modelData = LoadObjFile("resources", "plane.obj");

	// 10個分の行列
	static const int kInstanceCount = 10;
	ComPtr<ID3D12Resource> instanceBuffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW instanceVBV{};

	// 初期化
	{
		static const int WIDTH = 5;
		static const int HEIGHT = 5;
		static const int DEPTH = 4;
		static const float SPACING = 1.2f;

		static const int kInstanceCount = WIDTH * HEIGHT * DEPTH;

		uint32_t bufferSize = sizeof(InstanceData) * kInstanceCount;
		instanceBuffer = CreateBufferResource(device, bufferSize);

		InstanceData* map = nullptr;
		instanceBuffer->Map(0, nullptr, reinterpret_cast<void**>(&map));

		int idx = 0;
		for (int z = 0; z < DEPTH; z++) {
			for (int y = 0; y < HEIGHT; y++) {
				for (int x = 0; x < WIDTH; x++) {

					float px = x * SPACING;
					float py = y * SPACING;
					float pz = z * SPACING;

					// ★ 回転を追加（資料と同じ傾き）
					float ry = 0.6f;     // ← ここが一番大事
					float rx = 0.3f;     // ← 少し上に傾ける

					Matrix4x4 world = MakeAffineMatrix(
						{ 1.0f, 1.0f, 1.0f },
						{ rx, ry, 0.0f },     // ★ 板を斜めに傾ける！
						{ px, py, pz }
					);

					map[idx].world = world;
					idx++;
				}
			}
		}

		instanceBuffer->Unmap(0, nullptr);

		instanceVBV.BufferLocation = instanceBuffer->GetGPUVirtualAddress();
		instanceVBV.SizeInBytes = bufferSize;
		instanceVBV.StrideInBytes = sizeof(InstanceData);
	}


	// リソース作成
	ComPtr<ID3D12Resource> vertexResource = CreateBufferResource(device, sizeof(VertexData) * modelData.vertices.size());

	// リソース作成
	std::vector<ComPtr<ID3D12Resource>> textureResources;
	std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> textureSrvHandles;

	std::vector<VertexData> sphereVertices;
	std::vector<uint32_t> sphereIndices;
	GenerateSphereMesh(sphereVertices, sphereIndices, 32, 32);  // 分割数32で球生成

	// 頂点バッファ
	ComPtr<ID3D12Resource> vertexResourceSphere = CreateBufferResource(device, sizeof(VertexData) * sphereVertices.size());
	void* vertexDataSphere = nullptr;
	vertexResourceSphere->Map(0, nullptr, &vertexDataSphere);
	memcpy(vertexDataSphere, sphereVertices.data(), sizeof(VertexData) * sphereVertices.size());
	vertexResourceSphere->Unmap(0, nullptr);

	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSphere{};
	vertexBufferViewSphere.BufferLocation = vertexResourceSphere->GetGPUVirtualAddress();
	vertexBufferViewSphere.SizeInBytes = UINT(sizeof(VertexData) * sphereVertices.size());
	vertexBufferViewSphere.StrideInBytes = sizeof(VertexData);

	// インデックスバッファ
	ComPtr<ID3D12Resource> indexResourceSphere = CreateBufferResource(device, sizeof(uint32_t) * sphereIndices.size());
	void* indexDataSphere = nullptr;
	indexResourceSphere->Map(0, nullptr, &indexDataSphere);
	memcpy(indexDataSphere, sphereIndices.data(), sizeof(uint32_t) * sphereIndices.size());
	indexResourceSphere->Unmap(0, nullptr);

	D3D12_INDEX_BUFFER_VIEW indexBufferViewSphere{};
	indexBufferViewSphere.BufferLocation = indexResourceSphere->GetGPUVirtualAddress();
	indexBufferViewSphere.SizeInBytes = UINT(sizeof(uint32_t) * sphereIndices.size());
	indexBufferViewSphere.Format = DXGI_FORMAT_R32_UINT;

	// 頂点リソース用のヒープの設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; // アップロード用
	// 頂点リソースの設定
	D3D12_RESOURCE_DESC vertexResourceDesc{};
	// バッファリソース。テクスチャの場合はまた別の設定をする
	vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexResourceDesc.Width = sizeof(VertexData) * 6; // 頂点の数
	// バッファの場合はこれらは1にする決まり
	vertexResourceDesc.Height = 1;
	vertexResourceDesc.DepthOrArraySize = 1;
	vertexResourceDesc.MipLevels = 1;
	vertexResourceDesc.SampleDesc.Count = 1;
	// バッファの場合はこれにする決まり
	vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// 頂点バッファビューを作成
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	// リソースの先頭のアドレスから使う
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	// 使用するリソースサイズは頂点3つ分のサイズ
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
	// 1つの頂点のサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	// 頂点リソースにデータを書き込む
	VertexData* vertexData = nullptr;
	// 書き込むためのアドレスを取得
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	std::memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());
	vertexResource->Unmap(0, nullptr); // 書き込み完了したのでアンマップ

	// GPU上のマテリアルリソース一覧（マテリアル名で識別）
	std::unordered_map<std::string, ComPtr<ID3D12Resource>> materialResources;

	// CPU側のマテリアルポインタ一覧（ImGuiで編集用）
	std::unordered_map<std::string, Material*> materialDataList;

	// A マテリアル（32バイト必要）
	ComPtr<ID3D12Resource> materialResourceA = CreateBufferResource(device, sizeof(Material));
	Material* materialDataA = nullptr;
	materialResourceA->Map(0, nullptr, reinterpret_cast<void**>(&materialDataA));
	*materialDataA = { {1.0f, 1.0f, 1.0f, 1.0f},1 }; // Lighting有効

	// A WVP（128バイト必要）
	ComPtr<ID3D12Resource> wvpResourceA = CreateBufferResource(device, sizeof(TransformationMatrix));
	TransformationMatrix* wvpDataA = nullptr;
	wvpResourceA->Map(0, nullptr, reinterpret_cast<void**>(&wvpDataA));
	wvpDataA->WVP = MakeIdentity4x4();
	wvpDataA->World = MakeIdentity4x4();

	// B マテリアル
	ComPtr<ID3D12Resource> materialResourceB = CreateBufferResource(device, sizeof(Material));
	Material* materialDataB = nullptr;
	materialResourceB->Map(0, nullptr, reinterpret_cast<void**>(&materialDataB));
	*materialDataB = { {1.0f, 1.0f, 1.0f, 1.0f},1 }; // Lighting有効

	// B WVP
	ComPtr<ID3D12Resource> wvpResourceB = CreateBufferResource(device, sizeof(TransformationMatrix));
	TransformationMatrix* wvpDataB = nullptr;
	wvpResourceB->Map(0, nullptr, reinterpret_cast<void**>(&wvpDataB));
	wvpDataB->WVP = MakeIdentity4x4();
	wvpDataB->World = MakeIdentity4x4();


	// Sprite用のマテリアルリソースを作成する。今回はcolor1つ分のサイズを用意する
	ComPtr<ID3D12Resource> materialResourceSprite = CreateBufferResource(device, sizeof(Material));
	Material* materialDataSprite = nullptr;
	materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite));

	// UVTransformを単位行列で初期化する
	*materialDataSprite = {
		{1.0f, 1.0f, 1.0f, 1.0f}, // color
		false,                   // enableLighting
		{0.0f, 0.0f, 0.0f},      // padding
		MakeIdentity4x4()        // uvTransform 
	};


	// Index用
	ComPtr<ID3D12Resource> indexResourceSprite = CreateBufferResource(device, sizeof(uint32_t) * 6);
	// view
	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
	indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();
	indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6; // インデックスの数
	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;
	// インデックスデータをマップして書き込む
	uint32_t* indexDataSprite = nullptr;
	indexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSprite));
	// 使用例（6頂点分）
	indexDataSprite[0] = 0; // 左下1
	indexDataSprite[1] = 1; // 上1
	indexDataSprite[2] = 2; // 右下1
	indexDataSprite[3] = 1; // 左下2
	indexDataSprite[4] = 3; // 上2
	indexDataSprite[5] = 2; // 右下2

	// 平行光源のバッファを作成し、CPU 側から書き込めるようにする
	ComPtr<ID3D12Resource> directionalLightResource = CreateBufferResource(device, sizeof(DirectionalLight));
	DirectionalLight* directionalLightData = nullptr;
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));

	// 初期データ設定
	directionalLightData->color = { 1.0f, 1.0f, 1.0f };
	Vector3 dir = Normalize({ -1.0f, -1.0f, 0.0f });
	directionalLightData->direction = { dir.x, dir.y, dir.z, 0.0f };
	directionalLightData->intensity = 3.0f;

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
		  {0.5f, 0.5f, 0.5f},  // scale
		  {0.0f, 0.0f, 0.0f},  // rotate
		  {0.0f, 0.0f, 0.0f}   // translate
	};

	static Transform transformB = {
		  {0.5f, 0.5f, 0.5f},  // scale
		  {0.0f, 0.0f, 0.0f},  // rotate
		  {1.0f, 0.0f, 0.0f}   // translate
	};

	for (int i = 0; i < 10; i++) {
		particles[i] = MakeNewParticle(randomEngine);
	}



	// Textureを呼んで転送する
	DirectX::ScratchImage mipImages = LoadTexture("resources/circle.png");
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	ComPtr<ID3D12Resource> textureResource = CreateTextureResource(device, metadata);
	assert(textureResource);
	UploadTextureData(textureResource, mipImages);


	// 2枚目Textureを呼んで転送する
	DirectX::ScratchImage mipImages2 = LoadTexture("resources/monsterBall.png");
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	ComPtr<ID3D12Resource> textureResource2 = CreateTextureResource(device, metadata2);
	UploadTextureData(textureResource2, mipImages2);

	// 3枚目Textureを呼んで転送する
	DirectX::ScratchImage mipImages3 = LoadTexture("resources/checkerBoard.png");
	const DirectX::TexMetadata& metadata3 = mipImages3.GetMetadata();
	ComPtr<ID3D12Resource> textureResource3 = CreateTextureResource(device, metadata3);
	UploadTextureData(textureResource3, mipImages3);

	// metadataを基にSRVを作成する
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format; // フォーマット
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // コンポーネントマッピング
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels); // mipレベルの数

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format = metadata2.format; // フォーマット
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // コンポーネントマッピング
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels); // mipレベルの数

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc3{};
	srvDesc3.Format = metadata3.format;
	srvDesc3.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc3.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc3.Texture2D.MipLevels = UINT(metadata3.mipLevels);

	//SRVを作成するDescriptorHeapの先頭を取得する
	UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 1);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 1);
	// テクスチャのSRVを作成する
	device->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);
	// テクスチャのSRVを作成する
	device->CreateShaderResourceView(textureResource2.Get(), &srvDesc2, textureSrvHandleCPU2);

	// SRVの作成（3番目のスロット＝3番目のインデックス）
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU3 = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 3);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU3 = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 3);
	device->CreateShaderResourceView(textureResource3.Get(), &srvDesc3, textureSrvHandleCPU3);

	std::unordered_map<std::string, D3D12_GPU_DESCRIPTOR_HANDLE> textureHandleMap;
	std::vector<ComPtr<ID3D12Resource>> textureUploadBuffers; // アップロードバッファ保持用

	// uvChecker.png のSRV作成後に登録
	textureHandleMap[NormalizeTextureKey("circle.png")] = textureSrvHandleGPU;
	textureUploadBuffers.push_back(textureResource);

	// monsterBall.png のSRV作成後に登録
	textureHandleMap[NormalizeTextureKey("monsterBall.png")] = textureSrvHandleGPU2;
	textureUploadBuffers.push_back(textureResource2);

	// マップに登録（キーは .mtl に記載されてるファイル名に一致させる）
	textureHandleMap[NormalizeTextureKey("checkerBoard.png")] = textureSrvHandleGPU3;
	textureUploadBuffers.push_back(textureResource3);

	static UINT nextSrvIndex = 4; // 1,2,3 を使っているので次は 4 から

	auto ensureTextureSRV = [&](const std::string& texPath) -> D3D12_GPU_DESCRIPTOR_HANDLE {
		if (texPath.empty()) {
			// 何も指定が無いときの既定テクスチャ（例：uvChecker）
			return textureSrvHandleGPU;
		}

		std::string key = NormalizeTextureKey(texPath);
		if (auto it = textureHandleMap.find(key); it != textureHandleMap.end()) {
			return it->second; // 既に作ってあればそれを使う
		}

		// 読み込み → GPUリソース化 → SRV を新規作成
		DirectX::ScratchImage mip = LoadTexture(texPath);          // "resources/..." のフルパス想定
		const auto& md = mip.GetMetadata();
		ComPtr<ID3D12Resource> tex = CreateTextureResource(device, md);
		UploadTextureData(tex, mip);

		D3D12_SHADER_RESOURCE_VIEW_DESC sd{};
		sd.Format = md.format;
		sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		sd.Texture2D.MipLevels = UINT(md.mipLevels);

		auto cpu = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, nextSrvIndex);
		auto gpu = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, nextSrvIndex);
		device->CreateShaderResourceView(tex.Get(), &sd, cpu);

		textureHandleMap[key] = gpu;          // ハンドルを記録
		textureUploadBuffers.push_back(tex);  // リソース寿命を維持
		++nextSrvIndex;
		return gpu;
		};


	// Sprite用の頂点リソースを作る
	ComPtr<ID3D12Resource> vertexResourceSprite = CreateBufferResource(device, sizeof(VertexData) * 4);

	// 頂点バッファビューを作成
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	// リソースの先頭のアドレスから使う
	vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();
	// 使用するリソースのサイズは頂点6つ分のサイズ
	vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 4;
	// 1頂点あたりのサイズ
	vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

	// Spriteの頂点データを設定
	VertexData* vertexDataSprite = nullptr;
	assert(vertexResourceSprite); // 安全対策
	vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));
	// 頂点データ（左下, 左上, 右下, 右上）
	vertexDataSprite[0].position = { 0.0f, 360.0f, 0.0f, 1.0f }; // 左下
	vertexDataSprite[0].texcoord = { 0.0f, 1.0f };

	vertexDataSprite[1].position = { 0.0f, 0.0f, 0.0f, 1.0f }; // 左上
	vertexDataSprite[1].texcoord = { 0.0f, 0.0f };

	vertexDataSprite[2].position = { 640.0f, 360.0f, 0.0f, 1.0f }; // 右下
	vertexDataSprite[2].texcoord = { 1.0f, 1.0f };

	vertexDataSprite[3].position = { 640.0f, 0.0f, 0.0f, 1.0f }; // 右上
	vertexDataSprite[3].texcoord = { 1.0f, 0.0f };

	// Sprite用の法線を設定（-Z方向）
	for (int i = 0; i < 4; ++i) {
		vertexDataSprite[i].normal = { 0.0f, 0.0f, -1.0f };
	}


	// TransformationMatrix 構造体を使う
	ComPtr<ID3D12Resource> transformationMatrixResourceSprite = CreateBufferResource(device, sizeof(TransformationMatrix));
	TransformationMatrix* transformationMatrixDataSprite = nullptr;
	transformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite));
	// 単位行列で初期化
	transformationMatrixDataSprite->WVP = MakeIdentity4x4();
	transformationMatrixDataSprite->World = MakeIdentity4x4();
	// CPUで動かす用のTransformを作る
	Transform transformSprite{
		{1.0f, 1.0f, 1.0f},  // scale
		  {0.0f, 0.0f, 0.0f},  // rotate
		  {0.0f, 0.0f, 0.0f} // translate
	};

	Transform uvTransformSprite{
	{1.0f, 1.0f, 1.0f},  // scale
	{0.0f, 0.0f, 0.0f},  // rotate
	{0.0f, 0.0f, 0.0f},  // translate
	};

	bool useBillboard = false;


	// 音声データ読み込み
	SoundData soundData1 = SoundLoadWave("resources/Alarm01.wav");

	// モデルの種類を選択するための変数
	ModelType selectedModel = ModelType::InstancingPlane;
	bool shouldReloadModel = false;

	LightingMode lightingMode = LightingMode::HalfLambert;

	// キーの状態
	static BYTE key[256] = {};
	static BYTE keyPre[256] = {};

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

			// ImGuiのウィンドウを作成
			ImGui::ShowDemoWindow();

			ImGuiIO& io = ImGui::GetIO();
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // ゲームパッド操作を有効化

			// DirectInput ゲームパッド状態取得
			DIJOYSTATE gamepadState = {};
			if (gamepad) {
				gamepad->Acquire();
				gamepad->Poll();
				gamepad->GetDeviceState(sizeof(DIJOYSTATE), &gamepadState);
			}

			io.NavInputs[ImGuiNavInput_DpadUp] = (gamepadState.lY < -500) ? 1.0f : 0.0f;
			io.NavInputs[ImGuiNavInput_DpadDown] = (gamepadState.lY > 500) ? 1.0f : 0.0f;
			io.NavInputs[ImGuiNavInput_DpadLeft] = (gamepadState.lX < -500) ? 1.0f : 0.0f;
			io.NavInputs[ImGuiNavInput_DpadRight] = (gamepadState.lX > 500) ? 1.0f : 0.0f;

			// 決定（Aボタン想定：0番ボタン）
			io.NavInputs[ImGuiNavInput_Activate] = (gamepadState.rgbButtons[0] & 0x80) ? 1.0f : 0.0f;

			// キャンセル（Bボタン：1番）
			io.NavInputs[ImGuiNavInput_Cancel] = (gamepadState.rgbButtons[1] & 0x80) ? 1.0f : 0.0f;

			ImGui::Begin("Window");

			ImGui::SetItemDefaultFocus(); // ←追加！

			// モデル切り替え
			const char* modelItems[] = { "Plane","Sphere","UtahTeapot","StanfordBunny","MultiMesh","MultiMaterial","Fence","InstancingPlane"};
			int currentItem = static_cast<int>(selectedModel);
			if (ImGui::Combo("Model", &currentItem, modelItems, IM_ARRAYSIZE(modelItems))) {
				selectedModel = static_cast<ModelType>(currentItem);
				shouldReloadModel = true; // フラグを立てる
			}

			// モデルAのTransform
			if (ImGui::CollapsingHeader("Object A", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::DragFloat3("Translate", &transformA.translate.x, 0.01f, -2.0f, 2.0f);
				ImGui::DragFloat3("Rotate", &transformA.rotate.x, 0.01f, -6.0f, 6.0f);
				ImGui::DragFloat3("Scale", &transformA.scale.x, 0.01f, 0.0f, 4.0f);
				// Material
				if (ImGui::TreeNode("Material")) {
					ImGui::ColorEdit4("Color", &materialDataA->color.x);
					ImGui::TreePop();
				}
			}
			if (selectedModel == ModelType::Plane) {
				if (ImGui::CollapsingHeader("Object B", ImGuiTreeNodeFlags_DefaultOpen)) {
					ImGui::DragFloat3("Translate##B", &transformB.translate.x, 0.01f, -2.0f, 2.0f);
					ImGui::DragFloat3("Rotate##B", &transformB.rotate.x, 0.01f, -6.0f, 6.0f);
					ImGui::DragFloat3("Scale##B", &transformB.scale.x, 0.01f, 0.0f, 4.0f);

					if (ImGui::TreeNode("MaterialB")) {
						ImGui::ColorEdit4("ColorB", &materialDataB->color.x);
						ImGui::TreePop();
					}
				}
				// UV変換（Sprite用）
				if (ImGui::CollapsingHeader("Sprite UV")) {
					ImGui::DragFloat2("UVTranslate", &uvTransformSprite.translate.x, 0.01f, -10.0f, 10.0f);
					ImGui::DragFloat2("UVScale", &uvTransformSprite.scale.x, 0.01f, -10.0f, 10.0f);
					ImGui::SliderAngle("UVRotate", &uvTransformSprite.rotate.z);
				}
			}
			if (selectedModel == ModelType::MultiMaterial) {
				if (ImGui::CollapsingHeader("MultiMaterial", ImGuiTreeNodeFlags_DefaultOpen)) {
					int i = 0;
					for (auto& [name, matData] : materialDataList) {
						if (ImGui::TreeNode((name + "##" + std::to_string(i)).c_str())) {
							ImGui::DragFloat2(("UV Translate##" + name).c_str(), &matData->uvTransform.m[3][0], 0.01f, -10.0f, 10.0f);
							ImGui::DragFloat2(("UV Scale##" + name).c_str(), &matData->uvTransform.m[0][0], 0.01f, -10.0f, 10.0f);
							ImGui::SliderAngle(("UV Rotate##" + name).c_str(), &matData->uvTransform.m[0][1]); // 任意（角度表現）
							ImGui::ColorEdit4(("Color##" + name).c_str(), &matData->color.x);
							int lighting = static_cast<int>(matData->lightingMode);
							if (ImGui::Combo(("Lighting##" + name).c_str(), &lighting, "None\0Lambert\0HalfLambert\0")) {
								matData->lightingMode = lighting;
							}
						}
						++i;
					}
				}
			}

			static BlendMode uiBlend = BlendMode::Normal;
			if (ImGui::CollapsingHeader("Blend")) {
				const char* items[] = { "None","Normal","Add","Subtract","Multiply","Screen" };
				int idx = (int)uiBlend;
				if (ImGui::Combo("BlendMode", &idx, items, IM_ARRAYSIZE(items))) {
					uiBlend = (BlendMode)idx;
				}
			}

			ImGui::Checkbox("Use Billboard", &useBillboard);


			// 光の設定
			if (ImGui::CollapsingHeader("Light")) {
				const char* lightingItems[] = { "None", "Lambert", "HalfLambert" };
				int currentLighting = static_cast<int>(lightingMode);
				if (ImGui::Combo("Lighting Mode", &currentLighting, lightingItems, IM_ARRAYSIZE(lightingItems))) {
					lightingMode = static_cast<LightingMode>(currentLighting);
				}
				static Vector3 lightDirEdit = { directionalLightData->direction.x, directionalLightData->direction.y, directionalLightData->direction.z };
				if (ImGui::DragFloat3("Light Dir", &lightDirEdit.x, 0.01f, -1.0f, 1.0f)) {
					Vector3 normDir = Normalize(lightDirEdit);
					directionalLightData->direction = { normDir.x, normDir.y, normDir.z, 0.0f };
				}
				ImGui::DragFloat("Light Intensity", &directionalLightData->intensity, 0.01f, 0.0f, 10.0f);
				ImGui::ColorEdit3("Light Color", &directionalLightData->color.x);
			}

			ImGui::End();


			keyboard->Acquire();
			memcpy(keyPre, key, sizeof(key)); // 前の状態を保存
			keyboard->GetDeviceState(sizeof(key), key);

			// トリガー処理：スペースキーを押した瞬間だけ再生
			if (key[DIK_SPACE] && !keyPre[DIK_SPACE]) {
				SoundPlayWave(xAudio2.Get(), soundData1);
			}

			float dt = 1.0f / 60.0f;


			// WVP行列の計算
			Transform cameraTransform = { { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -5.0f } };
			Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
			Matrix4x4 viewMatrix = Inverse(cameraMatrix);
			Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(0.45f, float(kClientWidth) / float(kClientHeight), 0.1f, 100.0f);

			Matrix4x4 billboardMatrix = cameraMatrix;

			// 平行移動を消す（超重要）
			billboardMatrix.m[3][0] = 0.0f;
			billboardMatrix.m[3][1] = 0.0f;
			billboardMatrix.m[3][2] = 0.0f;


			// 三角形A
			Matrix4x4 worldMatrixA = MakeAffineMatrix(transformA.scale, transformA.rotate, transformA.translate);
			Matrix4x4 worldViewProjectionMatrixA = Multiply(worldMatrixA, Multiply(viewMatrix, projectionMatrix));
			wvpDataA->WVP = worldViewProjectionMatrixA;
			wvpDataA->World = worldMatrixA;
			materialDataA->lightingMode = static_cast<int32_t>(lightingMode);

			// 三角形B
			Matrix4x4 worldMatrixB = MakeAffineMatrix(transformB.scale, transformB.rotate, transformB.translate);
			Matrix4x4 worldViewProjectionMatrixB = Multiply(worldMatrixB, Multiply(viewMatrix, projectionMatrix));
			wvpDataB->WVP = worldViewProjectionMatrixB;
			wvpDataB->World = worldMatrixB;


			// Sprite用のWVPMを作る
			Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
			Matrix4x4 viewMatrixSprite = MakeIdentity4x4();
			Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(kClientWidth), float(kClientHeight), 0.0f, 100.0f);
			Matrix4x4 worldViewProjectionMatrixSprite = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));

			Matrix4x4 uvTransformMatrix = MakeScaleMatrix(uvTransformSprite.scale);
			uvTransformMatrix = Multiply(uvTransformMatrix, MakeRotateZMatrix(uvTransformSprite.rotate.z));
			uvTransformMatrix = Multiply(uvTransformMatrix, MakeTranslateMatrix(uvTransformSprite.translate));

			materialDataA->uvTransform = MakeIdentity4x4();
			materialDataSprite->uvTransform = uvTransformMatrix;

			// TransformationMatrixに正しく代入
			transformationMatrixDataSprite->WVP = worldViewProjectionMatrixSprite;
			transformationMatrixDataSprite->World = worldMatrixSprite;

			D3D12_GPU_DESCRIPTOR_HANDLE selectedTextureHandle = textureSrvHandleGPU;

			if ((selectedModel == ModelType::MultiMesh
				|| selectedModel == ModelType::MultiMaterial
				|| selectedModel == ModelType::Fence) && shouldReloadModel) {
				const char* fileName = GetModelFileName(selectedModel);
				multiModel = LoadObjFileMulti("resources", fileName);

				meshRenderList.clear();
				for (const auto& mesh : multiModel.meshes) {
					MeshRenderData renderData;
					renderData.vertexCount = mesh.vertices.size();
					renderData.name = mesh.name;
					renderData.materialName = mesh.materialName;

					renderData.vertexResource = CreateBufferResource(device, sizeof(VertexData) * mesh.vertices.size());
					void* vtxPtr = nullptr;
					renderData.vertexResource->Map(0, nullptr, &vtxPtr);
					memcpy(vtxPtr, mesh.vertices.data(), sizeof(VertexData) * mesh.vertices.size());
					renderData.vertexResource->Unmap(0, nullptr);

					renderData.vbView.BufferLocation = renderData.vertexResource->GetGPUVirtualAddress();
					renderData.vbView.SizeInBytes = UINT(sizeof(VertexData) * mesh.vertices.size());
					renderData.vbView.StrideInBytes = sizeof(VertexData);

					meshRenderList.push_back(renderData);
				}

				// ✅ マルチマテリアル初期化（ここを追加）
				materialResources.clear();
				materialDataList.clear();

				for (auto& [matName, mat] : multiModel.materials) {
					ComPtr<ID3D12Resource> resource = CreateBufferResource(device, sizeof(Material));
					Material* data = nullptr;
					resource->Map(0, nullptr, reinterpret_cast<void**>(&data));
					*data = mat;
					data->lightingMode = static_cast<int32_t>(lightingMode);

					materialResources[matName] = resource;
					materialDataList[matName] = data;

					if (!mat.textureFilePath.empty()) {
						ensureTextureSRV(mat.textureFilePath);
					}
				}

				shouldReloadModel = false;
			} else if (shouldReloadModel) {
				// 通常モデル（Plane, Sphereなど）
				const char* fileName = GetModelFileName(selectedModel);
				modelData = LoadObjFile("resources", fileName);

				vertexResource = CreateBufferResource(device, sizeof(VertexData) * modelData.vertices.size());
				void* vertexPtr = nullptr;
				vertexResource->Map(0, nullptr, &vertexPtr);
				memcpy(vertexPtr, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());
				vertexResource->Unmap(0, nullptr);

				vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
				vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
				vertexBufferView.StrideInBytes = sizeof(VertexData);

				shouldReloadModel = false;
			}
			
			if (selectedModel == ModelType::InstancingPlane) {
				for (int i = 0; i < kInstanceCount; i++) {

					particles[i].position += particles[i].velocity * dt;
					particles[i].currentLife += dt;

					float alpha = 1.0f - (particles[i].currentLife / particles[i].life);
					particles[i].color.w = alpha;

					if (particles[i].currentLife >= particles[i].life) {
						particles[i] = MakeNewParticle(randomEngine);
					}
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
			float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };
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
			commandList->SetPipelineState(pipelineByBlend[(int)uiBlend].Get());


			// 頂点バッファの設定
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			// 球の描画
			if (selectedModel == ModelType::Plane) {
				// Planeモデルを描画
				commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
				commandList->SetGraphicsRootConstantBufferView(0, materialResourceA->GetGPUVirtualAddress());
				commandList->SetGraphicsRootConstantBufferView(1, wvpResourceA->GetGPUVirtualAddress());
				commandList->SetGraphicsRootDescriptorTable(2, selectedTextureHandle);
				commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
				commandList->DrawInstanced(static_cast<UINT>(modelData.vertices.size()), 1, 0, 0);

				// さらにSphereも描画！
				commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSphere);
				commandList->IASetIndexBuffer(&indexBufferViewSphere);
				commandList->SetGraphicsRootConstantBufferView(0, materialResourceA->GetGPUVirtualAddress());
				commandList->SetGraphicsRootConstantBufferView(1, wvpResourceB->GetGPUVirtualAddress());
				commandList->SetGraphicsRootDescriptorTable(2, selectedTextureHandle); // Sphere用テクスチャ
				commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
				commandList->DrawIndexedInstanced(static_cast<UINT>(sphereIndices.size()), 1, 0, 0, 0);

				commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
				// Spriteの描画
				commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite);
				commandList->IASetIndexBuffer(&indexBufferViewSprite);
				// TransformationBufferの設定
				commandList->SetGraphicsRootConstantBufferView(0, materialResourceSprite->GetGPUVirtualAddress());
				commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSprite->GetGPUVirtualAddress());

				// 描画
				commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

			} else if (selectedModel == ModelType::Sphere) {
				// Sphereモデルを描画
				commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSphere);
				commandList->IASetIndexBuffer(&indexBufferViewSphere);
				commandList->SetGraphicsRootConstantBufferView(0, materialResourceA->GetGPUVirtualAddress());
				commandList->SetGraphicsRootConstantBufferView(1, wvpResourceA->GetGPUVirtualAddress());
				commandList->SetGraphicsRootDescriptorTable(2, selectedTextureHandle);
				commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
				commandList->DrawIndexedInstanced(static_cast<UINT>(sphereIndices.size()), 1, 0, 0, 0);
			} else if (selectedModel == ModelType::UtahTeapot) {
				// Teapotモデルを描画
				commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
				commandList->SetGraphicsRootConstantBufferView(0, materialResourceA->GetGPUVirtualAddress());
				commandList->SetGraphicsRootConstantBufferView(1, wvpResourceA->GetGPUVirtualAddress());
				commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU3);
				commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
				commandList->DrawInstanced(static_cast<UINT>(modelData.vertices.size()), 1, 0, 0);
			} else if (selectedModel == ModelType::StanfordBunny) {
				// Stanford Bunnyモデルを描画
				commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
				commandList->SetGraphicsRootConstantBufferView(0, materialResourceA->GetGPUVirtualAddress());
				commandList->SetGraphicsRootConstantBufferView(1, wvpResourceA->GetGPUVirtualAddress());
				commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
				commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
				commandList->DrawInstanced(static_cast<UINT>(modelData.vertices.size()), 1, 0, 0);
			} else if (selectedModel == ModelType::InstancingPlane) {

				InstanceData* map = nullptr;
				instanceBuffer->Map(0, nullptr, reinterpret_cast<void**>(&map));

				for (int i = 0; i < kInstanceCount; i++) {

					Matrix4x4 S = MakeScaleMatrix({ 0.3f, 0.3f, 0.3f });
					Matrix4x4 T = MakeTranslateMatrix(particles[i].position);

					Matrix4x4 R = MakeIdentity4x4();

					if (useBillboard) {
						R = billboardMatrix;   // カメラ回転
					}

					map[i].world = Multiply(S, Multiply(R, T));
					map[i].color = particles[i].color;
				}


				instanceBuffer->Unmap(0, nullptr);

				// 板ポリ（plane.obj）を使用
				commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

				D3D12_VERTEX_BUFFER_VIEW views[2] = {
				vertexBufferView,  // stream 0
				instanceVBV        // stream 1
				};

				commandList->IASetVertexBuffers(0, 2, views);


				// マテリアル設定
				commandList->SetGraphicsRootConstantBufferView(0, materialResourceA->GetGPUVirtualAddress());
				commandList->SetGraphicsRootConstantBufferView(1, wvpResourceA->GetGPUVirtualAddress());
				commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
				commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());

				// ★ インスタンス数ぶん描画（10個）
				commandList->DrawInstanced(
					static_cast<UINT>(modelData.vertices.size()),  // 1つの板ポリの頂点数
					kInstanceCount,                                // ← ここが instancing
					0, 0);
			}
			if (selectedModel == ModelType::MultiMesh
				|| selectedModel == ModelType::MultiMaterial
				|| selectedModel == ModelType::Fence) {                     // ★追加
				for (const auto& mesh : meshRenderList) {
					// マテリアルに紐づくテクスチャを取得（なければ既定にフォールバック）
					D3D12_GPU_DESCRIPTOR_HANDLE texHandle = textureSrvHandleGPU;
					if (auto it = multiModel.materials.find(mesh.materialName);
						it != multiModel.materials.end()) {
						texHandle = ensureTextureSRV(it->second.textureFilePath); // ★差し替え
					}

					// 描画
					commandList->IASetVertexBuffers(0, 1, &mesh.vbView);

					if (auto matResourceIt = materialResources.find(mesh.materialName);
						matResourceIt != materialResources.end()) {
						commandList->SetGraphicsRootConstantBufferView(0, matResourceIt->second->GetGPUVirtualAddress());
					} else {
						commandList->SetGraphicsRootConstantBufferView(0, materialResourceA->GetGPUVirtualAddress());
					}

					commandList->SetGraphicsRootConstantBufferView(1, wvpResourceA->GetGPUVirtualAddress());
					commandList->SetGraphicsRootDescriptorTable(2, texHandle);
					commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
					commandList->DrawInstanced(static_cast<UINT>(mesh.vertexCount), 1, 0, 0);
				}
			}



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

	// 出力ウィンドウへの文字出力
	OutputDebugStringA("Hello, DirectX!\n");

	// 解放処理
	xAudio2.Reset(); // XAudio2の解放
	SoundUnload(&soundData1); // 音声データの解放
	CloseHandle(fenceEvent);
	if (gamepad) {
		gamepad->Unacquire();
		gamepad->Release();
		gamepad = nullptr;
	}
	if (directInput) {
		directInput->Release();
		directInput = nullptr;
	}
	if (vertexShaderBlob) { vertexShaderBlob->Release(); vertexShaderBlob = nullptr; }
	if (pixelShaderBlob) { pixelShaderBlob->Release();  pixelShaderBlob = nullptr; }
	if (signatureBlob) { signatureBlob->Release();    signatureBlob = nullptr; }
	if (errorBlob) { errorBlob->Release();        errorBlob = nullptr; }
	if (includeHandler) { includeHandler->Release();   includeHandler = nullptr; }
	if (dxcCompiler) { dxcCompiler->Release();      dxcCompiler = nullptr; }
	if (dxcUtils) { dxcUtils->Release();         dxcUtils = nullptr; }


	CloseWindow(hwnd);

	// ImGuiの終了処理
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	// COMの終了処理
	CoUninitialize();
	return 0;
}