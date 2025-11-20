#include <vector>
#include <algorithm>
#include <cmath>
#include <format>
#include <d3dcompiler.h>
#include <d3d12sdklayers.h>
#include <dxgidebug.h>
#include <dxcapi.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <unordered_map>
#include "engine/3d/ResourceObject.h"
#include "engine/io/Input.h"
#include "engine/base/WinApp.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/Logger.h"
#include "engine/base/StringUtility.h"
#include "engine/base/D3DResourceLeakChecker.h"
#include "engine/2d/SpriteCommon.h"
#include "engine/2d/Sprite.h"
#include "engine/base/Math.h"
#include <wrl/client.h>
#include <xaudio2.h>
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"
#include "DirectXTex.h"

#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "xaudio2.lib")

using namespace Microsoft::WRL;
using Logger::Log;
using StringUtility::ConvertString;
using namespace Math;

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
	MultiMaterial
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

// 関数の作成

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
        }
        else if (identifier == "Kd") {
            // 拡散反射色
            s >> currentMaterial.color.x >> currentMaterial.color.y >> currentMaterial.color.z;
            currentMaterial.color.w = 1.0f;
        }
        else if (identifier == "map_Kd") {
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
	//HRESULT result;
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
	default: return "plane.obj";
	}
}

auto NormalizeTextureKey = [](const std::string& path) -> std::string {
	std::string filename = std::filesystem::path(path).filename().string();
	std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
	return filename;
	};

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	D3DResourceLeakChecker leakcheck;

	// ポインタ
	WinApp* winApp = nullptr;

	// Windowsアプリの初期化
	winApp = new WinApp();
	winApp->Initialize();

	// ポインタ
	DirectXCommon* dxCommon = nullptr;

	// DirectXの初期化
	dxCommon = new DirectXCommon();
	dxCommon->Initialize(winApp);

	// ===== DirectXCommon から必要なものを引っ張ってくる =====
	HRESULT hr = S_OK;

	ID3D12Device* device = dxCommon->GetDevice();
	ID3D12CommandQueue* commandQueue = dxCommon->GetCommandQueue();
	ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();

	ID3D12DescriptorHeap* srvDescriptorHeap = dxCommon->GetSRVHeap();
	UINT descriptorSizeSRV = dxCommon->GetSRVDescriptorSize();

	ID3D12DescriptorHeap* rtvDescriptorHeap = dxCommon->GetRTVHeap();
	ID3D12DescriptorHeap* dsvDescriptorHeap = dxCommon->GetDSVHeap();

	const D3D12_VIEWPORT& viewport = dxCommon->GetViewport();
	const D3D12_RECT& scissorRect = dxCommon->GetScissorRect();

	ID3D12Fence* fence = dxCommon->GetFence();
	UINT64& fenceValue = dxCommon->GetFenceValue();
	HANDLE       fenceEvent = dxCommon->GetFenceEvent();

	IDxcUtils* dxcUtils = dxCommon->GetDxcUtils();
	IDxcCompiler3* dxcCompiler = dxCommon->GetDxcCompiler();
	IDxcIncludeHandler* includeHandler = dxCommon->GetDxcIncludeHandler();

	// SwapChain の情報（Imgui初期化用に BufferCount を取る）
	DXGI_SWAP_CHAIN_DESC swapChainDesc{};
	dxCommon->GetSwapChain()->GetDesc(&swapChainDesc);

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

	// POSITION (float4 = 16 バイト)
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = 0;

	// TEXCOORD (float2 = 8 バイト)
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = 16;

	// NORMAL (float3 = 12 バイト)
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = 24;

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
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// Shaderのコンパイル
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob =
		dxCommon->CompileShader(L"shaders/Object3D.VS.hlsl", L"vs_6_0");
	assert(vertexShaderBlob); // null チェック

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob =
		dxCommon->CompileShader(L"shaders/Object3D.PS.hlsl", L"ps_6_0");
	assert(pixelShaderBlob);

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

	ComPtr<IXAudio2> xAudio2 = nullptr;
	HRESULT result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	assert(SUCCEEDED(result));

	IXAudio2MasteringVoice* masterVoice = nullptr;
	result = xAudio2->CreateMasteringVoice(&masterVoice);
	assert(SUCCEEDED(result));

	// DepthStencilの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// ポインタ
	Input* input = nullptr;

	// 入力の初期化
	input = new Input();
	input->Initialize(winApp);

	SpriteCommon* spriteCommon = nullptr;

	// スプライト共通部の初期化
	spriteCommon = new SpriteCommon;
	spriteCommon->Initialize(dxCommon); // ★ 修正

	// 実際に生成
	ComPtr<ID3D12PipelineState> graphicsPipelineState = nullptr;
	hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));

	// モデルデータの読み込み
	ModelData modelData = LoadObjFile("resources", "plane.obj");

	// リソース作成
	ComPtr<ID3D12Resource> vertexResource =
		dxCommon->CreateBufferResource(sizeof(VertexData) * modelData.vertices.size());


	// リソース作成
	std::vector<ComPtr<ID3D12Resource>> textureResources;
	std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> textureSrvHandles;

	std::vector<VertexData> sphereVertices;
	std::vector<uint32_t> sphereIndices;
	GenerateSphereMesh(sphereVertices, sphereIndices, 32, 32);  // 分割数32で球生成

	// 頂点バッファ
	ComPtr<ID3D12Resource> vertexResourceSphere = dxCommon->CreateBufferResource(sizeof(VertexData) * sphereVertices.size());
	void* vertexDataSphere = nullptr;
	vertexResourceSphere->Map(0, nullptr, &vertexDataSphere);
	memcpy(vertexDataSphere, sphereVertices.data(), sizeof(VertexData) * sphereVertices.size());
	vertexResourceSphere->Unmap(0, nullptr);

	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSphere{};
	vertexBufferViewSphere.BufferLocation = vertexResourceSphere->GetGPUVirtualAddress();
	vertexBufferViewSphere.SizeInBytes = UINT(sizeof(VertexData) * sphereVertices.size());
	vertexBufferViewSphere.StrideInBytes = sizeof(VertexData);

	// インデックスバッファ
	ComPtr<ID3D12Resource> indexResourceSphere = dxCommon->CreateBufferResource(sizeof(uint32_t) * sphereIndices.size());
	void* indexDataSphere = nullptr;
	indexResourceSphere->Map(0, nullptr, &indexDataSphere);
	memcpy(indexDataSphere, sphereIndices.data(), sizeof(uint32_t) * sphereIndices.size());
	indexResourceSphere->Unmap(0, nullptr);

	D3D12_INDEX_BUFFER_VIEW indexBufferViewSphere{};
	indexBufferViewSphere.BufferLocation = indexResourceSphere->GetGPUVirtualAddress();
	indexBufferViewSphere.SizeInBytes = UINT(sizeof(uint32_t) * sphereIndices.size());
	indexBufferViewSphere.Format = DXGI_FORMAT_R32_UINT;


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
	ComPtr<ID3D12Resource> materialResourceA = dxCommon->CreateBufferResource(sizeof(Material));
	Material* materialDataA = nullptr;
	materialResourceA->Map(0, nullptr, reinterpret_cast<void**>(&materialDataA));
	*materialDataA = { {1.0f, 1.0f, 1.0f, 1.0f},1 }; // Lighting有効

	// A WVP（128バイト必要）
	ComPtr<ID3D12Resource> wvpResourceA = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
	TransformationMatrix* wvpDataA = nullptr;
	wvpResourceA->Map(0, nullptr, reinterpret_cast<void**>(&wvpDataA));
	wvpDataA->WVP = MakeIdentity4x4();
	wvpDataA->World = MakeIdentity4x4();

	// B マテリアル
	ComPtr<ID3D12Resource> materialResourceB = dxCommon->CreateBufferResource(sizeof(Material));
	Material* materialDataB = nullptr;
	materialResourceB->Map(0, nullptr, reinterpret_cast<void**>(&materialDataB));
	*materialDataB = { {1.0f, 1.0f, 1.0f, 1.0f},1 }; // Lighting有効

	// B WVP
	ComPtr<ID3D12Resource> wvpResourceB = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
	TransformationMatrix* wvpDataB = nullptr;
	wvpResourceB->Map(0, nullptr, reinterpret_cast<void**>(&wvpDataB));
	wvpDataB->WVP = MakeIdentity4x4();
	wvpDataB->World = MakeIdentity4x4();

	// 平行光源のバッファを作成し、CPU 側から書き込めるようにする
	ComPtr<ID3D12Resource> directionalLightResource = dxCommon->CreateBufferResource(sizeof(DirectionalLight));
	DirectionalLight* directionalLightData = nullptr;
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));

	// 初期データ設定
	directionalLightData->color = { 1.0f, 1.0f, 1.0f };
	Vector3 dir = Normalize({ -1.0f, -1.0f, 0.0f });
	directionalLightData->direction = { dir.x, dir.y, dir.z, 0.0f };
	directionalLightData->intensity = 3.0f;

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

	// Textureを呼んで転送する
	DirectX::ScratchImage mipImages = DirectXCommon::LoadTexture("resources/uvChecker.png");
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	ComPtr<ID3D12Resource> textureResource =
		dxCommon->CreateTextureResource(metadata);
	assert(textureResource);
	dxCommon->UploadTextureData(textureResource, mipImages);


	// 2枚目Textureを呼んで転送する
	DirectX::ScratchImage mipImages2 = DirectXCommon::LoadTexture("resources/monsterBall.png");
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	ComPtr<ID3D12Resource> textureResource2 =
		dxCommon->CreateTextureResource(metadata2);
	dxCommon->UploadTextureData(textureResource2, mipImages2);

	// 3枚目Textureを呼んで転送する
	DirectX::ScratchImage mipImages3 = DirectXCommon::LoadTexture("resources/checkerBoard.png");
	const DirectX::TexMetadata& metadata3 = mipImages3.GetMetadata();
	ComPtr<ID3D12Resource> textureResource3 =
		dxCommon->CreateTextureResource(metadata3);
	dxCommon->UploadTextureData(textureResource3, mipImages3);


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
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU =
		dxCommon->GetSRVCPUDescriptorHandle(1);   // 1番目のSRV
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU =
		dxCommon->GetSRVGPUDescriptorHandle(1);
	// テクスチャのSRVを作成する
	device->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = dxCommon->GetSRVCPUDescriptorHandle(2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = dxCommon->GetSRVGPUDescriptorHandle(2);
	// テクスチャのSRVを作成する
	device->CreateShaderResourceView(textureResource2.Get(), &srvDesc2, textureSrvHandleCPU2);

	// SRVの作成（3番目のスロット＝3番目のインデックス）
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU3 = dxCommon->GetSRVCPUDescriptorHandle(3);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU3 = dxCommon->GetSRVGPUDescriptorHandle(3);
	device->CreateShaderResourceView(textureResource3.Get(), &srvDesc3, textureSrvHandleCPU3);

	std::unordered_map<std::string, D3D12_GPU_DESCRIPTOR_HANDLE> textureHandleMap;
	std::vector<ComPtr<ID3D12Resource>> textureUploadBuffers; // アップロードバッファ保持用

	// uvChecker.png のSRV作成後に登録
	textureHandleMap[NormalizeTextureKey("uvChecker.png")] = textureSrvHandleGPU;
	textureUploadBuffers.push_back(textureResource);

	// monsterBall.png のSRV作成後に登録
	textureHandleMap[NormalizeTextureKey("monsterBall.png")] = textureSrvHandleGPU2;
	textureUploadBuffers.push_back(textureResource2);

	// マップに登録（キーは .mtl に記載されてるファイル名に一致させる）
	textureHandleMap[NormalizeTextureKey("checkerBoard.png")] = textureSrvHandleGPU3;
	textureUploadBuffers.push_back(textureResource3);

	std::vector<Sprite> sprites;
	sprites.resize(1);

	sprites[0].Initialize(spriteCommon);
	sprites[0].SetTexture(textureSrvHandleGPU);
	sprites[0].SetPosition({ 0,0 });
	sprites[0].SetSize({ 640,360 });

	// 音声データ読み込み
	SoundData soundData1 = SoundLoadWave("resources/Alarm01.wav");

	// モデルの種類を選択するための変数
	ModelType selectedModel = ModelType::Plane; // 初期はPlane
	bool shouldReloadModel = false;

	LightingMode lightingMode = LightingMode::HalfLambert;

	

	// ウィンドウのxボタンが押されるまでループ
	while (true) {
		// Windowsにメッセージが来てたら最優先で処理させる
		if (winApp->ProcessMessage()) {
			break;
		}
		// ゲームの処理
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// ImGuiのウィンドウを作成
		ImGui::ShowDemoWindow();

		ImGui::Begin("Window");

		ImGui::SetItemDefaultFocus(); // ←追加！
		
		// モデル切り替え
		const char* modelItems[] = { "Plane", "Sphere", "UtahTeapot", "StanfordBunny", "MultiMesh", "MultiMaterial" };
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
				ImGui::ColorEdit3("Color", &materialDataA->color.x);
				ImGui::TreePop();
			}
		}
		if (selectedModel == ModelType::Plane) {
			if (ImGui::CollapsingHeader("Object B", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::DragFloat3("Translate##B", &transformB.translate.x, 0.01f, -2.0f, 2.0f);
				ImGui::DragFloat3("Rotate##B", &transformB.rotate.x, 0.01f, -6.0f, 6.0f);
				ImGui::DragFloat3("Scale##B", &transformB.scale.x, 0.01f, 0.0f, 4.0f);

				if (ImGui::TreeNode("MaterialB")) {
					ImGui::ColorEdit3("ColorB", &materialDataB->color.x);
					ImGui::TreePop();
				}
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
						ImGui::ColorEdit3(("Color##" + name).c_str(), &matData->color.x);
						int lighting = static_cast<int>(matData->lightingMode);
						if (ImGui::Combo(("Lighting##" + name).c_str(), &lighting, "None\0Lambert\0HalfLambert\0")) {
							matData->lightingMode = lighting;
						}
					}
					++i;
				}
			}
		}

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

		// キーボード入力の更新
		input->Update();

		// トリガー処理：スペースキーを押した瞬間だけ再生
		if (input->TriggerKey(DIK_SPACE)) {
			SoundPlayWave(xAudio2.Get(), soundData1);
		}


		// WVP行列の計算
		Transform cameraTransform = { { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -5.0f } };
		Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
		Matrix4x4 viewMatrix = Inverse(cameraMatrix);
		Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(0.45f, float(WinApp::kClientWidth) / float(WinApp::kClientHeight), 0.1f, 100.0f);

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


		materialDataA->uvTransform = MakeIdentity4x4();

		D3D12_GPU_DESCRIPTOR_HANDLE selectedTextureHandle = textureSrvHandleGPU;

		if ((selectedModel == ModelType::MultiMesh || selectedModel == ModelType::MultiMaterial) && shouldReloadModel) {
			const char* fileName = GetModelFileName(selectedModel);
			multiModel = LoadObjFileMulti("resources", fileName);

			meshRenderList.clear();
			for (const auto& mesh : multiModel.meshes) {
				MeshRenderData renderData;
				renderData.vertexCount = mesh.vertices.size();
				renderData.name = mesh.name;
				renderData.materialName = mesh.materialName;

				renderData.vertexResource = dxCommon->CreateBufferResource( sizeof(VertexData) * mesh.vertices.size());
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
				ComPtr<ID3D12Resource> resource = dxCommon->CreateBufferResource( sizeof(Material));
				Material* data = nullptr;
				resource->Map(0, nullptr, reinterpret_cast<void**>(&data));
				*data = mat;
				data->lightingMode = static_cast<int32_t>(lightingMode);

				materialResources[matName] = resource;
				materialDataList[matName] = data;
			}

			shouldReloadModel = false;
		} else if (shouldReloadModel) {
			// 通常モデル（Plane, Sphereなど）
			const char* fileName = GetModelFileName(selectedModel);
			modelData = LoadObjFile("resources", fileName);

			vertexResource = dxCommon->CreateBufferResource(sizeof(VertexData) * modelData.vertices.size());
			void* vertexPtr = nullptr;
			vertexResource->Map(0, nullptr, &vertexPtr);
			memcpy(vertexPtr, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());
			vertexResource->Unmap(0, nullptr);

			vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
			vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
			vertexBufferView.StrideInBytes = sizeof(VertexData);

			shouldReloadModel = false;
		}

		for (auto& s : sprites) s.Update();


		// ImGuiの描画
		ImGui::Render();

		dxCommon->PreDraw();

		// 3D用 RootSignature と PSO をセット
		commandList->SetGraphicsRootSignature(rootSignature.Get());
		commandList->SetPipelineState(graphicsPipelineState.Get());

		// 頂点バッファの設定
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


		// 球の描画
		if (selectedModel == ModelType::Plane) {
			// Plane
			commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
			commandList->SetGraphicsRootConstantBufferView(0, materialResourceA->GetGPUVirtualAddress());
			commandList->SetGraphicsRootConstantBufferView(1, wvpResourceA->GetGPUVirtualAddress());
			commandList->SetGraphicsRootDescriptorTable(2, selectedTextureHandle);
			commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
			commandList->DrawInstanced(static_cast<UINT>(modelData.vertices.size()), 1, 0, 0);

			// Sphere
			commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSphere);
			commandList->IASetIndexBuffer(&indexBufferViewSphere);
			commandList->SetGraphicsRootConstantBufferView(0, materialResourceA->GetGPUVirtualAddress());
			commandList->SetGraphicsRootConstantBufferView(1, wvpResourceB->GetGPUVirtualAddress());
			commandList->SetGraphicsRootDescriptorTable(2, selectedTextureHandle);
			commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
			commandList->DrawIndexedInstanced(static_cast<UINT>(sphereIndices.size()), 1, 0, 0, 0);

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
		}if (selectedModel == ModelType::MultiMesh || selectedModel == ModelType::MultiMaterial) {
			for (const auto& mesh : meshRenderList) {
				// テクスチャキーを取得
				std::string texKey = "none";
				auto it = multiModel.materials.find(mesh.materialName);
				if (it != multiModel.materials.end()) {
					texKey = NormalizeTextureKey(it->second.textureFilePath);
				}

				D3D12_GPU_DESCRIPTOR_HANDLE texHandle = textureSrvHandleGPU;
				if (textureHandleMap.count(texKey)) {
					texHandle = textureHandleMap[texKey];
				} else {
					Log("❌ textureHandleMapに " + texKey + " が存在しない");
				}

				// 描画
				commandList->IASetVertexBuffers(0, 1, &mesh.vbView);

				// ImGuiで操作されたマテリアルバッファを使う
				auto matResourceIt = materialResources.find(mesh.materialName);
				if (matResourceIt != materialResources.end()) {
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

		// ===== スプライト描画 =====
		spriteCommon->CommonDrawSetting();
		for (auto& s : sprites) s.Draw();

		// ImGuiの描画
		ImGui_ImplDX12_RenderDrawData(
			ImGui::GetDrawData(),
			dxCommon->GetCommandList()
		);


		dxCommon->PostDraw();
		
	}

	// 出力ウィンドウへの文字出力
	OutputDebugStringA("Hello, DirectX!\n");

	// 解放処理
	xAudio2.Reset(); // XAudio2の解放
	CloseHandle(fenceEvent);
	delete input;

	// windowsAPIの終了
	winApp->Finalize();
	// WindowsAPI解放
	delete winApp;

	// ImGuiの終了
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	delete dxCommon;



	return 0;
}