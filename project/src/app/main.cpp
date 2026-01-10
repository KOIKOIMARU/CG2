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
#include "engine/3d/Camera.h"
#include "engine/base/SrvManager.h"
#include "engine/3d/ParticleManager.h"
#include "engine/3d/ParticleEmitter.h"
#include "engine/base/ImGuiManager.h"
#include <wrl/client.h>
#include <xaudio2.h>
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

	SrvManager* srvManager = new SrvManager();
	srvManager->Initialize(dxCommon);

	ImGuiManager* imguiManager = new ImGuiManager();
	imguiManager->Initialize(winApp, dxCommon, srvManager);

	// ★ テクスチャマネージャの初期化（ここがスライドの「呼び出し」）
	TextureManager::GetInstance()->Initialize(dxCommon, srvManager);


	// 3Dオブジェクト共通部
	Object3dCommon* object3dCommon = new Object3dCommon();
	object3dCommon->Initialize(dxCommon, srvManager);


	// ===== Model 共通部 =====
	ModelCommon* modelCommon = new ModelCommon();
	modelCommon->Initialize(dxCommon, srvManager);


	// ===== カメラ生成 =====
	Camera* camera = new Camera();
	camera->SetRotate({ 0.3f, 0.0f, 0.0f });
	camera->SetTranslate({ 0.0f, 4.0f, -10.0f });

	// ★ デフォルトカメラに設定（資料どおり）
	object3dCommon->SetDefaultCamera(camera);


	ModelManager::GetInstance()->Initialize(dxCommon,srvManager);
	ModelManager::GetInstance()->LoadModel("plane.obj");

	Object3d* object3d = new Object3d();
	object3d->Initialize(object3dCommon);
	object3d->SetModel("plane.obj");



	// ===== DirectXCommon から必要なものを引っ張ってくる =====
	HRESULT hr = S_OK;

	ID3D12Device* device = dxCommon->GetDevice();
	ID3D12CommandQueue* commandQueue = dxCommon->GetCommandQueue();
	ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();

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

	// SwapChain の情報
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
	spriteCommon = new SpriteCommon();
	spriteCommon->Initialize(dxCommon, srvManager);



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
	uint32_t texUvCheckerIndex =
		TextureManager::GetInstance()->GetSrvIndex("resources/uvChecker.png");

	uint32_t texMonsterBallIndex =
		TextureManager::GetInstance()->GetSrvIndex("resources/monsterBall.png");

	uint32_t texCheckerBoardIndex =
		TextureManager::GetInstance()->GetSrvIndex("resources/checkerBoard.png");


	// --- Sprite 初期化 ---
	std::vector<Sprite> sprites;
	sprites.resize(1);

	// ★ ファイルパスを渡して初期化（内部で TextureManager を使う）
	sprites[0].Initialize(spriteCommon, "resources/uvChecker.png");
	sprites[0].SetPosition({ 0, 0 });
	sprites[0].SetSize({ 640, 360 });

	ParticleManager::GetInstance()->Initialize(dxCommon, srvManager);

	ParticleManager::GetInstance()->CreateParticleGroup(
		"test",
		"resources/circle.png" // ← 実在するテクスチャ
	);


	// エミッタ生成
	ParticleEmitter emitter(
		"test",                 // グループ名
		{ 0.0f, 2.0f, 0.0f },      // 位置
		0.1f,                   // 発生間隔
		5                        // 1回の発生数
	);



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
		//ImGui_ImplDX12_NewFrame();
		//ImGui_ImplWin32_NewFrame();
		//ImGui::NewFrame();

		imguiManager->Begin();

		// ここにUI追加

		imguiManager->End(); // ← Updateの最後が妥当


		// キーボード入力の更新
		input->Update();

		// トリガー処理：スペースキーを押した瞬間だけ再生
		if (input->TriggerKey(DIK_SPACE)) {
			SoundPlayWave(xAudio2.Get(), soundData1);
		}

		// 更新
		float deltaTime = dxCommon->GetDeltaTime(); // or 自前計算

		camera->Update();

		object3d->Update();

		emitter.Update(deltaTime);

		ParticleManager::GetInstance()->Update(
			camera->GetViewMatrix(),
			camera->GetProjectionMatrix()
		);



		for (auto& s : sprites) s.Update();


		// ImGuiの描画
		//ImGui::Render();

		dxCommon->PreDraw();

		srvManager->PreDraw();

		ParticleManager::GetInstance()->Draw();


		object3dCommon->CommonDrawSetting();

		object3d->Draw();



		// ===== スプライト描画 =====
		spriteCommon->CommonDrawSetting();
		for (auto& s : sprites) s.Draw();


		// ★ImGui 描画（PostDrawの前）
		imguiManager->Draw();

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
	delete camera;
	// ★ImGuiを最初に後始末（依存が多いので）
	imguiManager->Finalize();
	delete imguiManager;

	// その後に他のシステム
	ParticleManager::GetInstance()->Finalize();
	TextureManager::GetInstance()->Destroy();
	ModelManager::GetInstance()->Finalize();

	delete srvManager;
	delete dxCommon;

	// WinAppは最後
	winApp->Finalize();
	delete winApp;



#ifdef _DEBUG
	D3DResourceLeakChecker leakChecker;
#endif

	return 0;
}