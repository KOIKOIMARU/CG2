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
#include "engine/3d/TextureManager.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/ModelCommon.h"
#include "engine/3d/Model.h"
#include "engine/3d/ModelManager.h"
#include <wrl/client.h>
#include <xaudio2.h>
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"
#include "DirectXTex.h"
#include <DirectXMath.h>

#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "xaudio2.lib")

using namespace Microsoft::WRL;
using Logger::Log;
using StringUtility::ConvertString;
using namespace Math;
using namespace DirectX;



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

std::unordered_map<std::string, D3D12_GPU_DESCRIPTOR_HANDLE> textureHandleMap;

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

//std::unordered_map<std::string, Material> LoadMaterialTemplateMulti(
//    const std::string& directoryPath,
//    const std::string& filename)
//{
//    std::unordered_map<std::string, Material> materials;
//    std::ifstream file(directoryPath + "/" + filename);
//    assert(file.is_open());
//
//    std::string line;
//    std::string currentMaterialName;
//    Material currentMaterial{};
//
//    while (std::getline(file, line)) {
//        std::istringstream s(line);
//        std::string identifier;
//        s >> identifier;
//
//        if (identifier == "newmtl") {
//            // 直前のマテリアルを保存
//            if (!currentMaterialName.empty()) {
//                materials[currentMaterialName] = currentMaterial;
//            }
//
//            // 新しいマテリアル名
//            s >> currentMaterialName;
//            currentMaterial = Material(); // 初期化
//            currentMaterial.color = { 1.0f, 1.0f, 1.0f, 1.0f };
//            currentMaterial.lightingMode = 1; // Lambertなど
//            currentMaterial.uvTransform = MakeIdentity4x4();
//        }
//        else if (identifier == "Kd") {
//            // 拡散反射色
//            s >> currentMaterial.color.x >> currentMaterial.color.y >> currentMaterial.color.z;
//            currentMaterial.color.w = 1.0f;
//        }
//        else if (identifier == "map_Kd") {
//            std::string textureFilename;
//            s >> textureFilename;
//            currentMaterial.textureFilePath = directoryPath + "/" + textureFilename;
//        }
//    }
//
//    // 最後のマテリアルを保存
//    if (!currentMaterialName.empty()) {
//        materials[currentMaterialName] = currentMaterial;
//    }
//
//    return materials;
//}



//MultiModelData LoadObjFileMulti(const std::string& directoryPath, const std::string& filename) {
//	MultiModelData modelData;
//
//	std::vector<Vector4> positions;
//	std::vector<Vector2> texcoords;
//	std::vector<Vector3> normals;
//
//	std::ifstream file(directoryPath + "/" + filename);
//	assert(file.is_open());
//
//	std::string line;
//	std::string currentMeshName = "default";
//	std::string currentMaterialName = "default"; // 現在のマテリアル名
//	Mesh currentMesh;
//
//	while (std::getline(file, line)) {
//		std::istringstream s(line);
//		std::string identifier;
//		s >> identifier;
//
//		if (identifier == "v") {
//			Vector4 pos; s >> pos.x >> pos.y >> pos.z;
//			pos.z *= -1.0f;
//			pos.w = 1.0f;
//			positions.push_back(pos);
//		} else if (identifier == "vt") {
//			Vector2 uv; s >> uv.x >> uv.y;
//			texcoords.push_back(uv);
//		} else if (identifier == "vn") {
//			Vector3 n; s >> n.x >> n.y >> n.z;
//			n.z *= -1.0f;
//			normals.push_back(n);
//		} else if (identifier == "f") {
//			VertexData tri[3];
//			for (int i = 0; i < 3; ++i) {
//				std::string vtx;
//				s >> vtx;
//				std::istringstream vs(vtx);
//				uint32_t idx[3] = {};
//				for (int j = 0; j < 3; ++j) {
//					std::string val;
//					std::getline(vs, val, '/');
//					idx[j] = std::stoi(val);
//				}
//				tri[i] = {
//					positions[idx[0] - 1],
//					{ texcoords[idx[1] - 1].x, 1.0f - texcoords[idx[1] - 1].y },
//					normals[idx[2] - 1]
//				};
//			}
//			currentMesh.vertices.push_back(tri[2]);
//			currentMesh.vertices.push_back(tri[1]);
//			currentMesh.vertices.push_back(tri[0]);
//		} else if (identifier == "g" || identifier == "o") {
//			if (!currentMesh.vertices.empty()) {
//				currentMesh.name = currentMeshName;
//				currentMesh.materialName = currentMaterialName; // 使用中のマテリアル名を記録
//				modelData.meshes.push_back(currentMesh);
//				currentMesh = Mesh(); // 次のMeshへ
//			}
//			s >> currentMeshName;
//		} else if (identifier == "mtllib") {
//			std::string mtl;
//			s >> mtl;
//			modelData.materials = LoadMaterialTemplateMulti(directoryPath, mtl); // マテリアル複数対応版
//		} else if (identifier == "usemtl") {
//			// 現在のマテリアル名を更新
//			s >> currentMaterialName;
//
//			// もし現メッシュに頂点があれば、いったん保存してマテリアル名を更新
//			if (!currentMesh.vertices.empty()) {
//				currentMesh.name = currentMeshName;
//				currentMesh.materialName = currentMaterialName;
//				modelData.meshes.push_back(currentMesh);
//				currentMesh = Mesh(); // 次のメッシュへ切り替え
//			}
//		}
//
//	}
//
//	if (!currentMesh.vertices.empty()) {
//		currentMesh.name = currentMeshName;
//		currentMesh.materialName = currentMaterialName;
//		modelData.meshes.push_back(currentMesh);
//	}
//
//	return modelData;
//}


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

	// ★ テクスチャマネージャの初期化（ここがスライドの「呼び出し」）
	TextureManager::GetInstance()->Initialize(dxCommon->GetDevice(), dxCommon);

	// 3Dオブジェクト共通部
	Object3dCommon* object3dCommon = new Object3dCommon();
	object3dCommon->Initialize(dxCommon);


	ModelManager::GetInstance()->Initialize(dxCommon);
	ModelManager::GetInstance()->LoadModel("plane.obj");

	Object3d* object3d = new Object3d();
	object3d->Initialize(object3dCommon);
	object3d->SetModel("plane.obj");



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


	ComPtr<IXAudio2> xAudio2 = nullptr;
	HRESULT result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	assert(SUCCEEDED(result));

	IXAudio2MasteringVoice* masterVoice = nullptr;
	result = xAudio2->CreateMasteringVoice(&masterVoice);
	assert(SUCCEEDED(result));

	// ポインタ
	Input* input = nullptr;

	// 入力の初期化
	input = new Input();
	input->Initialize(winApp);

	SpriteCommon* spriteCommon = nullptr;

	// スプライト共通部の初期化
	spriteCommon = new SpriteCommon;
	spriteCommon->Initialize(dxCommon); // ★ 修正


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


	// --- テクスチャ読み込み ---
	TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
	TextureManager::GetInstance()->LoadTexture("resources/monsterBall.png");
	TextureManager::GetInstance()->LoadTexture("resources/checkerBoard.png");

	// ★ それぞれの GPU ハンドルを取っておく
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleUvChecker =
		TextureManager::GetInstance()->GetTextureHandle("resources/uvChecker.png");
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleMonsterBall =
		TextureManager::GetInstance()->GetTextureHandle("resources/monsterBall.png");
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleCheckerBoard =
		TextureManager::GetInstance()->GetTextureHandle("resources/checkerBoard.png");

	// --- Sprite 初期化 ---
	std::vector<Sprite> sprites;
	sprites.resize(1);

	// ★ ファイルパスを渡して初期化（内部で TextureManager を使う）
	sprites[0].Initialize(spriteCommon, "resources/uvChecker.png");
	sprites[0].SetPosition({ 0, 0 });
	sprites[0].SetSize({ 640, 360 });


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
		
		//// モデル切り替え
		//const char* modelItems[] = { "Plane", "Sphere", "UtahTeapot", "StanfordBunny", "MultiMesh", "MultiMaterial" };
		//int currentItem = static_cast<int>(selectedModel);
		//if (ImGui::Combo("Model", &currentItem, modelItems, IM_ARRAYSIZE(modelItems))) {
		//	selectedModel = static_cast<ModelType>(currentItem);
		//	shouldReloadModel = true; // フラグを立てる
		//}

		//// モデルAのTransform
		//if (ImGui::CollapsingHeader("Object A", ImGuiTreeNodeFlags_DefaultOpen)) {
		//	ImGui::DragFloat3("Translate", &transformA.translate.x, 0.01f, -2.0f, 2.0f);
		//	ImGui::DragFloat3("Rotate", &transformA.rotate.x, 0.01f, -6.0f, 6.0f);
		//	ImGui::DragFloat3("Scale", &transformA.scale.x, 0.01f, 0.0f, 4.0f);
		//	// Material
		//	if (ImGui::TreeNode("Material")) {
		//		ImGui::ColorEdit3("Color", &materialDataA->color.x);
		//		ImGui::TreePop();
		//	}
		//}
		//if (selectedModel == ModelType::Plane) {
		//	if (ImGui::CollapsingHeader("Object B", ImGuiTreeNodeFlags_DefaultOpen)) {
		//		ImGui::DragFloat3("Translate##B", &transformB.translate.x, 0.01f, -2.0f, 2.0f);
		//		ImGui::DragFloat3("Rotate##B", &transformB.rotate.x, 0.01f, -6.0f, 6.0f);
		//		ImGui::DragFloat3("Scale##B", &transformB.scale.x, 0.01f, 0.0f, 4.0f);

		//		if (ImGui::TreeNode("MaterialB")) {
		//			ImGui::ColorEdit3("ColorB", &materialDataB->color.x);
		//			ImGui::TreePop();
		//		}
		//	}
		//}
		//if (selectedModel == ModelType::MultiMaterial) {
		//	if (ImGui::CollapsingHeader("MultiMaterial", ImGuiTreeNodeFlags_DefaultOpen)) {
		//		int i = 0;
		//		for (auto& [name, matData] : materialDataList) {
		//			if (ImGui::TreeNode((name + "##" + std::to_string(i)).c_str())) {
		//				ImGui::DragFloat2(("UV Translate##" + name).c_str(), &matData->uvTransform.m[3][0], 0.01f, -10.0f, 10.0f);
		//				ImGui::DragFloat2(("UV Scale##" + name).c_str(), &matData->uvTransform.m[0][0], 0.01f, -10.0f, 10.0f);
		//				ImGui::SliderAngle(("UV Rotate##" + name).c_str(), &matData->uvTransform.m[0][1]); // 任意（角度表現）
		//				ImGui::ColorEdit3(("Color##" + name).c_str(), &matData->color.x);
		//				int lighting = static_cast<int>(matData->lightingMode);
		//				if (ImGui::Combo(("Lighting##" + name).c_str(), &lighting, "None\0Lambert\0HalfLambert\0")) {
		//					matData->lightingMode = lighting;
		//				}
		//			}
		//			++i;
		//		}
		//	}
		//}

		//// 光の設定
		//if (ImGui::CollapsingHeader("Light")) {
		//	const char* lightingItems[] = { "None", "Lambert", "HalfLambert" };
		//	int currentLighting = static_cast<int>(lightingMode);
		//	if (ImGui::Combo("Lighting Mode", &currentLighting, lightingItems, IM_ARRAYSIZE(lightingItems))) {
		//		lightingMode = static_cast<LightingMode>(currentLighting);
		//	}
		//	static Vector3 lightDirEdit = { directionalLightData->direction.x, directionalLightData->direction.y, directionalLightData->direction.z };
		//	if (ImGui::DragFloat3("Light Dir", &lightDirEdit.x, 0.01f, -1.0f, 1.0f)) {
		//		Vector3 normDir = Normalize(lightDirEdit);
		//		directionalLightData->direction = { normDir.x, normDir.y, normDir.z };
		//	}
		//	ImGui::DragFloat("Light Intensity", &directionalLightData->intensity, 0.01f, 0.0f, 10.0f);
		//	ImGui::ColorEdit3("Light Color", &directionalLightData->color.x);
		//}

		ImGui::End();

		// キーボード入力の更新
		input->Update();

		// トリガー処理：スペースキーを押した瞬間だけ再生
		if (input->TriggerKey(DIK_SPACE)) {
			SoundPlayWave(xAudio2.Get(), soundData1);
		}

		// 更新
		object3d->Update();

		D3D12_GPU_DESCRIPTOR_HANDLE selectedTextureHandle = textureSrvHandleUvChecker;

		//if ((selectedModel == ModelType::MultiMesh || selectedModel == ModelType::MultiMaterial) && shouldReloadModel) {
		//	const char* fileName = GetModelFileName(selectedModel);
		//	multiModel = LoadObjFileMulti("resources", fileName);

		//	meshRenderList.clear();
		//	for (const auto& mesh : multiModel.meshes) {
		//		MeshRenderData renderData;
		//		renderData.vertexCount = mesh.vertices.size();
		//		renderData.name = mesh.name;
		//		renderData.materialName = mesh.materialName;

		//		renderData.vertexResource = dxCommon->CreateBufferResource( sizeof(VertexData) * mesh.vertices.size());
		//		void* vtxPtr = nullptr;
		//		renderData.vertexResource->Map(0, nullptr, &vtxPtr);
		//		memcpy(vtxPtr, mesh.vertices.data(), sizeof(VertexData) * mesh.vertices.size());
		//		renderData.vertexResource->Unmap(0, nullptr);

		//		renderData.vbView.BufferLocation = renderData.vertexResource->GetGPUVirtualAddress();
		//		renderData.vbView.SizeInBytes = UINT(sizeof(VertexData) * mesh.vertices.size());
		//		renderData.vbView.StrideInBytes = sizeof(VertexData);

		//		meshRenderList.push_back(renderData);
		//	}

		//	// ✅ マルチマテリアル初期化（ここを追加）
		//	materialResources.clear();
		//	materialDataList.clear();

		//	for (auto& [matName, mat] : multiModel.materials) {
		//		ComPtr<ID3D12Resource> resource = dxCommon->CreateBufferResource( sizeof(Material));
		//		Material* data = nullptr;
		//		resource->Map(0, nullptr, reinterpret_cast<void**>(&data));
		//		*data = mat;
		//		data->lightingMode = static_cast<int32_t>(lightingMode);

		//		materialResources[matName] = resource;
		//		materialDataList[matName] = data;
		//	}

		//	// ★ テクスチャハンドルも更新
		//	textureHandleMap.clear();

		//	for (auto& [matName, mat] : multiModel.materials) {
		//		if (!mat.textureFilePath.empty()) {
		//			// 正規化したキー（ファイル名のみ・小文字）を作る
		//			std::string key = NormalizeTextureKey(mat.textureFilePath);

		//			// 読み込んで GPU ハンドル取得
		//			TextureManager::GetInstance()->LoadTexture(mat.textureFilePath);
		//			textureHandleMap[key] =
		//				TextureManager::GetInstance()->GetTextureHandle(mat.textureFilePath);
		//		}
		//	}


		//	shouldReloadModel = false;
		//} else if (shouldReloadModel) {
		//	// 通常モデル（Plane, Sphereなど）
		//	const char* fileName = GetModelFileName(selectedModel);
		//	modelData = LoadObjFile("resources", fileName);

		//	vertexResource = dxCommon->CreateBufferResource(sizeof(VertexData) * modelData.vertices.size());
		//	void* vertexPtr = nullptr;
		//	vertexResource->Map(0, nullptr, &vertexPtr);
		//	memcpy(vertexPtr, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());
		//	vertexResource->Unmap(0, nullptr);

		//	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
		//	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
		//	vertexBufferView.StrideInBytes = sizeof(VertexData);

		//	shouldReloadModel = false;
		//}

		for (auto& s : sprites) s.Update();


		// ImGuiの描画
		ImGui::Render();

		dxCommon->PreDraw();

		object3dCommon->CommonDrawSetting();

		object3d->Draw();

		//// 球の描画
		//if (selectedModel == ModelType::Plane) {
		//	// Plane
		//	commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		//	commandList->SetGraphicsRootConstantBufferView(0, materialResourceA->GetGPUVirtualAddress());
		//	commandList->SetGraphicsRootConstantBufferView(1, wvpResourceA->GetGPUVirtualAddress());
		//	commandList->SetGraphicsRootDescriptorTable(2, selectedTextureHandle);
		//	commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
		//	commandList->DrawInstanced(static_cast<UINT>(modelData.vertices.size()), 1, 0, 0);

		//	// Sphere
		//	commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSphere);
		//	commandList->IASetIndexBuffer(&indexBufferViewSphere);
		//	commandList->SetGraphicsRootConstantBufferView(0, materialResourceA->GetGPUVirtualAddress());
		//	commandList->SetGraphicsRootConstantBufferView(1, wvpResourceB->GetGPUVirtualAddress());
		//	commandList->SetGraphicsRootDescriptorTable(2, selectedTextureHandle);
		//	commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
		//	commandList->DrawIndexedInstanced(static_cast<UINT>(sphereIndices.size()), 1, 0, 0, 0);

		//} else if (selectedModel == ModelType::Sphere) {
		//	// Sphereモデルを描画
		//	commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSphere);
		//	commandList->IASetIndexBuffer(&indexBufferViewSphere);
		//	commandList->SetGraphicsRootConstantBufferView(0, materialResourceA->GetGPUVirtualAddress());
		//	commandList->SetGraphicsRootConstantBufferView(1, wvpResourceA->GetGPUVirtualAddress());
		//	commandList->SetGraphicsRootDescriptorTable(2, selectedTextureHandle);
		//	commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
		//	commandList->DrawIndexedInstanced(static_cast<UINT>(sphereIndices.size()), 1, 0, 0, 0);
		//} else if (selectedModel == ModelType::UtahTeapot) {
		//	// Teapotモデルを描画
		//	commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		//	commandList->SetGraphicsRootConstantBufferView(0, materialResourceA->GetGPUVirtualAddress());
		//	commandList->SetGraphicsRootConstantBufferView(1, wvpResourceA->GetGPUVirtualAddress());
		//	commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleCheckerBoard);
		//	commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
		//	commandList->DrawInstanced(static_cast<UINT>(modelData.vertices.size()), 1, 0, 0);
		//} else if (selectedModel == ModelType::StanfordBunny) {
		//	// Stanford Bunnyモデルを描画
		//	commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		//	commandList->SetGraphicsRootConstantBufferView(0, materialResourceA->GetGPUVirtualAddress());
		//	commandList->SetGraphicsRootConstantBufferView(1, wvpResourceA->GetGPUVirtualAddress());
		//	commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleUvChecker);
		//	commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
		//	commandList->DrawInstanced(static_cast<UINT>(modelData.vertices.size()), 1, 0, 0);
		//}if (selectedModel == ModelType::MultiMesh || selectedModel == ModelType::MultiMaterial) {
		//	for (const auto& mesh : meshRenderList) {
		//		// テクスチャキーを取得
		//		std::string texKey = "none";
		//		auto it = multiModel.materials.find(mesh.materialName);
		//		if (it != multiModel.materials.end()) {
		//			texKey = NormalizeTextureKey(it->second.textureFilePath);
		//		}

		//		D3D12_GPU_DESCRIPTOR_HANDLE texHandle = textureSrvHandleUvChecker;
		//		if (textureHandleMap.count(texKey)) {
		//			texHandle = textureHandleMap[texKey];
		//		} else {
		//			Log("❌ textureHandleMapに " + texKey + " が存在しない");
		//		}

		//		// 描画
		//		commandList->IASetVertexBuffers(0, 1, &mesh.vbView);

		//		// ImGuiで操作されたマテリアルバッファを使う
		//		auto matResourceIt = materialResources.find(mesh.materialName);
		//		if (matResourceIt != materialResources.end()) {
		//			commandList->SetGraphicsRootConstantBufferView(0, matResourceIt->second->GetGPUVirtualAddress());
		//		} else {
		//			commandList->SetGraphicsRootConstantBufferView(0, materialResourceA->GetGPUVirtualAddress());
		//		}

		//		commandList->SetGraphicsRootConstantBufferView(1, wvpResourceA->GetGPUVirtualAddress());
		//		commandList->SetGraphicsRootDescriptorTable(2, texHandle);
		//		commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());

		//		commandList->DrawInstanced(static_cast<UINT>(mesh.vertexCount), 1, 0, 0);
		//	}
		//}

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
	delete object3d;          // シーン依存
	delete object3dCommon;    // 基盤システム
	delete input;
	ModelManager::GetInstance()->Finalize();

	// windowsAPIの終了
	winApp->Finalize();
	// WindowsAPI解放
	delete winApp;

	// ImGuiの終了
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	TextureManager::GetInstance()->Finalize();

	delete dxCommon;

	return 0;
}