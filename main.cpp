#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <cstdint>
#include <vector>
#include <unordered_map>   
#include <algorithm>
#include <cctype>          
#include <cmath>
#include <cstring>
#include <string>
#include <format>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <dxcapi.h>
#include <cassert>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <wrl/client.h>
#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定
#include <dinput.h>
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

struct GPUMaterial {
	Vector4 color;           
	int32_t enableLighting; 
	float    pad_[3];       
	Matrix4x4 uvTransform;
};
static_assert(sizeof(GPUMaterial) == 96, "MaterialCB size mismatch");


// Material は CPU 専用に（必要なら）
struct Material {
	Vector4 color;
	int32_t lightingMode;
	float padding[3];
	Matrix4x4 uvTransform;
	std::string textureFilePath; // CPU専用用途だけで使うならOK（CBVに流さない）
};

struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
	Matrix4x4 WorldInverseTranspose; // 追加
};

struct DirectionalLight {
	Vector4 color;
	Vector4 direction;
	float intensity;
	Vector3 padding; // ← float3 paddingで16バイト境界に揃える
};

struct GPUDirectionalLight {
	Vector4 color;
	Vector4 direction; // wは0でOK
	float   intensity;
	float   pad_[3];
};
static_assert(sizeof(GPUDirectionalLight) == 48, "DirLightCB size mismatch");

struct Lightning {
	bool active;
	float time;
	float life;

	std::vector<Vector3> points; // ← 稲妻を構成する点のリスト
};
// 稲妻エフェクト用
std::vector<Lightning> gLightningList;

struct MaterialData {
	std::string textureFilePath;
};

struct ModelData {
	std::vector<VertexData> vertices; // 頂点データ
	MaterialData material; // マテリアルデータ
};

struct Player {
	Vector3 pos{ 0,0,0 };
	Vector3 vel{ 0,0,0 };
	Vector3 scale{ 1,1,1 };
	Vector3 rot{ 0,0,0 };
	float hp = 100.0f;
	float maxHp = 100.0f;

	bool facingRight = true;

	// 地上関連
	bool onGround = false;

	// 二段ジャンプ
	bool canDoubleJump = false;

	// 壁関連
	bool onWallLeft = false;
	bool onWallRight = false;
	float wallSlideSpeed = -3.0f; // ゆっくり落ちる速度
	float wallKickLock = 0.0f;

	// 無敵時間
	bool invincible = false;
	float invincibleTime = 0.0f;
	float invincibleDuration = 1.0f; // 1秒無敵
	float blinkTimer = 0.0f;

	// ノックバック
	float knockbackPower = 8.0f;
	float knockbackUp = 5.0f;

	// 空中ダッシュ
	bool isDashing = false;
	bool canAirDash = true;
	float dashSpeed = 18.0f;   // ← ★ 距離アップ
	float dashTime = 0.18f;    // ← ★ 時間延長
	float dashTimer = 0.0f;

	float charge = 0.0f;          // チャージ量（0〜1）
	float maxChargeTime = 1.0f;   // ここまで溜めると最大
	bool isCharging = false;      // Z押しっぱ？
} gPlayer;


enum class GameState { Title, Playing, Clear, GameOver };
static GameState gState = GameState::Title;

struct AABB {
	float x, y, w, h; // 左下原点でOK（Y=0が床）
};
static inline bool Intersects(const AABB& a, const AABB& b) {
	return !(a.x + a.w < b.x || b.x + b.w < a.x || a.y + a.h < b.y || b.y + b.h < a.y);
}

struct Enemy {
	Vector3 pos{ 8.0f, 0.0f, 0.0f }; // 右の方に1体
	Vector3 size{ 1.2f, 1.2f, 1.0f };
	bool alive = true;
} gEnemy;

struct Boss {
	Vector3 pos{ 10,1,0 };
	Vector3 vel{ 0,0,0 };

	float sizeX = 1.8f;
	float sizeY = 1.8f;

	float hp = 120.0f;
	float maxHp = 120.0f;
	bool  alive = true;

	float walkSpeed = 1.5f;  // そのままでOK
	float dashSpeed = 3.0f;  // OK

	// ★ 距離設定をタイル単位に合わせる
	float attackRange = 2.0f;   // 2マス以内で近接攻撃
	float dashRange = 15.0f;  // 15マス以内でダッシュ開始候補
	float keepDistance = 6.0f;   // 6マス以内には近づきすぎとみなして止まる

	float attackCooldown = 5.0f;
	float dashInstantSpeed = 0.0f;


	enum class State {
		Idle,
		Approach,
		Attack,
		DashPrep,
		Dash
	} state = State::Idle;

	bool facingRight = true;
} gBoss;



struct Bullet {
	Vector3 pos{};
	Vector3 vel{};
	float   life = 2.0f; // 秒
	float damage = 5.0f;
	Vector3 size{ 0.3f, 0.3f, 1.0f };
	bool alive = false;
};
static std::vector<Bullet> gBullets;

struct ChargeEffect {
	bool active = false;
	float timer = 0.0f;
	float scale = 0.5f; // 基本スケール
	float lastSparkTime = 0.0f;
};
ChargeEffect gChargeFx;

struct Spark {
	float time;      // 生存時間
	float life;      // 最大寿命
	Vector3 p0;      // 開始点
	Vector3 p1;      // 終了点
	bool active;
};

std::vector<Spark> gSparks;


struct HitEffect {
	Vector3 pos;
	float life = 0.2f;  // 0.2秒で消える
	bool alive = true;
};
std::vector<HitEffect> gHitEffects;


// プレイヤーの見た目/当たり判定サイズ（plane を矩形として使う）
static Vector3 kPlayerSize{ 1.0f, 1.6f, 1.0f };

// ===== タイルマップ（見た目用） =====
static constexpr float TILE = 1.0f;
static const int MAP_W = 20;
static const int MAP_H = 12;

static int gMap[MAP_H][MAP_W] = {

	// ======= 天井 =======
	{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},

	// ======= 空間 =======
	{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
	{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},

	// ======= 上段足場 =======
	{1,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,1},

	// ======= 空間 =======
	{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
	{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
	{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},

	// ======= 中段足場 =======
	{1,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1},

	// ======= 空間 =======
	{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
	{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
	{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},

	// ======= 床 =======
	{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
};


// ==== タイル衝突判定用ヘルパー ====

// マップインデックスから「当たり判定アリかどうか」を返す
inline bool IsSolidTileByIndex(int mapX, int mapY) {
	// マップ外は「壁扱い」にしておく（外に出ないようにしたい場合）
	if (mapX < 0 || mapX >= MAP_W || mapY < 0 || mapY >= MAP_H) {
		return true;
	}
	return gMap[mapY][mapX] == 1;
}

// ワールド座標 (wx, wy) が含まれるタイルが当たり判定アリかどうか
inline bool IsSolidAtWorld(float wx, float wy) {
	int tileX = static_cast<int>(std::floor(wx / TILE));
	int tileYWorld = static_cast<int>(std::floor(wy / TILE)); // 下からのタイル番号
	int mapY = MAP_H - 1 - tileYWorld; // gMap のインデックスに変換

	return IsSolidTileByIndex(tileX, mapY);
}


// 前方宣言（gMaterialTemplate で使うため）
Matrix4x4 MakeIdentity4x4();


// ★ グローバル定数の「前」に置く
constexpr size_t AlignCBSize(size_t size) {
	constexpr size_t kAlign = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT; // 256
	return (size + (kAlign - 1)) & ~(kAlign - 1);
}


// ======== CBリングバッファ用（グローバル） ========
constexpr size_t kMaxDraws = 4096;
constexpr size_t kMatSize = AlignCBSize(sizeof(GPUMaterial));          // 256B
constexpr size_t kXformSize = AlignCBSize(sizeof(TransformationMatrix)); // 256B

// 大きいCB
Microsoft::WRL::ComPtr<ID3D12Resource> gMaterialCB;
Microsoft::WRL::ComPtr<ID3D12Resource> gXformCB;

// マップ先
uint8_t* gMaterialMapped = nullptr;
uint8_t* gXformMapped = nullptr;

// フレーム内カーソル
size_t gMaterialCursor = 0;
size_t gXformCursor = 0;

// 既存の「テンプレート」的に使うCPU側データ（ImGuiでいじる値の置き場）
GPUMaterial gMaterialTemplate = { {1,1,1,1}, 1, {}, MakeIdentity4x4() };



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

enum class LightingMode {
	None,
	Lambert,
	HalfLambert,
};
 
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

Matrix4x4 Transpose(const Matrix4x4& m) {
	Matrix4x4 r{};
	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++)
			r.m[i][j] = m.m[j][i];
	return r;
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

Vector3 Normalize(const Vector3& v) {
	float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	if (length == 0.0f) return { 0.0f, 0.0f, 0.0f };
	return { v.x / length, v.y / length, v.z / length };
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

// ベクトルユーティリティ
static inline Vector3 Sub(const Vector3& a, const Vector3& b) { return { a.x - b.x,a.y - b.y,a.z - b.z }; }
static inline float Dot(const Vector3& a, const Vector3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline Vector3 Cross(const Vector3& a, const Vector3& b) {
	return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

// Z=0の平面に位置を拘束（2.5D）
static inline void ConstrainToZ0(Vector3& p) { p.z = 0.0f; }

// 左手座標系のLookAt行列（ビュー行列を直接返す）
static Matrix4x4 MakeLookAtMatrixLH(const Vector3& eye, const Vector3& target, const Vector3& upHint) {
	Vector3 zaxis = Normalize(Sub(target, eye));           // 前（+Z 方向）
	Vector3 xaxis = Normalize(Cross(upHint, zaxis));        // 右
	Vector3 yaxis = Cross(zaxis, xaxis);                    // 上

	Matrix4x4 m{};
	m.m[0][0] = xaxis.x; m.m[1][0] = xaxis.y; m.m[2][0] = xaxis.z; m.m[3][0] = -Dot(xaxis, eye);
	m.m[0][1] = yaxis.x; m.m[1][1] = yaxis.y; m.m[2][1] = yaxis.z; m.m[3][1] = -Dot(yaxis, eye);
	m.m[0][2] = zaxis.x; m.m[1][2] = zaxis.y; m.m[2][2] = zaxis.z; m.m[3][2] = -Dot(zaxis, eye);
	m.m[0][3] = 0.0f;    m.m[1][3] = 0.0f;    m.m[2][3] = 0.0f;    m.m[3][3] = 1.0f;
	return m;
}

void ResetGame() {
	// -----------------------
	// プレイヤー初期化
	// -----------------------
	gPlayer.pos = { 1.0f, 1.0f, 0.0f };
	gPlayer.vel = { 0,0,0 };
	gPlayer.onGround = true;
	gPlayer.canDoubleJump = true;
	gPlayer.canAirDash = true;
	gPlayer.isCharging = false;
	gPlayer.charge = 0.0f;
	gPlayer.facingRight = true;

	// ★ HP とダメージ関連をリセット
	gPlayer.hp = gPlayer.maxHp;
	gPlayer.invincible = false;
	gPlayer.invincibleTime = 0.0f;

	// ノックバック値が固定ならそのまま
	// gPlayer.knockbackPower = 6.0f; など
	// gPlayer.knockbackUp = 6.0f;

	// ★ 壁ジャンプや状態も安全のため初期化
	gPlayer.onWallLeft = false;
	gPlayer.onWallRight = false;
	gPlayer.wallKickLock = 0.0f;
	gPlayer.isDashing = false;
	gPlayer.dashTimer = 0.0f;

	// -----------------------
	// ボス初期化（最重要）
	// -----------------------
	gBoss.pos = { 10.0f, 1.0f, 0.0f };  // マップに合う位置へ
	gBoss.vel = { 0,0,0 };

	gBoss.hp = gBoss.maxHp;
	gBoss.alive = true;

	gBoss.state = Boss::State::Idle;
	gBoss.attackCooldown = 5.0f;
	gBoss.facingRight = true;

	// -----------------------
	// 弾リセット
	// -----------------------
	gBullets.clear();
	// -----------------------
// チャージエフェクト初期化
// -----------------------
	gChargeFx.active = false;
	gChargeFx.timer = 0.0f;
	gChargeFx.lastSparkTime = 0.0f;

	// 稲妻エフェクト
	gLightningList.clear();

	// （使っているなら）通常スパーク
	gSparks.clear();

	// プレイヤーの点滅状態
	gPlayer.blinkTimer = 0.0f;

}



// ===== 矩形描画（plane.objを矩形として使う） =====
// ※ materialCB / transformCB / lightCB は WinMain で作ったリソースを渡す
void DrawQuad(
	ID3D12GraphicsCommandList* cmd,
	const D3D12_VERTEX_BUFFER_VIEW& vbv,
	ID3D12Resource* materialCB, GPUMaterial* mat,
	ID3D12Resource* transformCB, TransformationMatrix* wvp,
	const Matrix4x4& view, const Matrix4x4& proj,
	float x, float y, float w, float h,
	const Vector4& rgba,
	D3D12_GPU_DESCRIPTOR_HANDLE textureHandle,
	ID3D12Resource* lightCB)
{
	// 左下原点→中心原点補正（plane が原点中心想定）
	Vector3 scl{ w, h, 1.0f };
	Vector3 rot{ 0,0,0 };
	Vector3 trs{ x + w * 0.5f, y + h * 0.5f, 0.0f };

	mat->color = rgba;

	Matrix4x4 world = MakeAffineMatrix(scl, rot, trs);
	wvp->World = world;
	wvp->WVP = Multiply(Multiply(world, view), proj);
	wvp->WorldInverseTranspose = Transpose(Inverse(world));

	cmd->IASetVertexBuffers(0, 1, &vbv);
	cmd->SetGraphicsRootConstantBufferView(0, materialCB->GetGPUVirtualAddress());
	cmd->SetGraphicsRootConstantBufferView(1, transformCB->GetGPUVirtualAddress());
	cmd->SetGraphicsRootDescriptorTable(2, textureHandle);
	cmd->SetGraphicsRootConstantBufferView(3, lightCB->GetGPUVirtualAddress());
	cmd->DrawInstanced(6, 1, 0, 0); // plane.obj が2三角形=6頂点想定
}

// ===== タイル描画 =====
void DrawTileMap(
	ID3D12GraphicsCommandList* cmd,
	const D3D12_VERTEX_BUFFER_VIEW& vbv,
	ID3D12Resource* materialCB, GPUMaterial* mat,
	ID3D12Resource* transformCB, TransformationMatrix* wvp,
	const Matrix4x4& view, const Matrix4x4& proj,
	D3D12_GPU_DESCRIPTOR_HANDLE textureHandle,
	ID3D12Resource* lightCB)
{
	for (int y = 0; y < MAP_H; ++y) {
		for (int x = 0; x < MAP_W; ++x) {
			if (gMap[y][x] == 1) {
				DrawQuad(cmd, vbv, materialCB, mat, transformCB, wvp,
					view, proj, x * TILE, y * TILE, TILE, TILE,
					{ 0.25f,0.25f,0.28f,1.0f }, textureHandle, lightCB);
			}
		}
	}
}

struct GfxModel {
	ModelData               data;
	ComPtr<ID3D12Resource> vb;
	D3D12_VERTEX_BUFFER_VIEW vbv{};
	UINT vertexCount = 0;
};

static GfxModel CreateGfxModel(
	ComPtr<ID3D12Device>& device,
	const std::string& dir, const std::string& filename)
{
	GfxModel m;
	m.data = LoadObjFile(dir, filename);
	m.vertexCount = static_cast<UINT>(m.data.vertices.size());

	m.vb = CreateBufferResource(device, sizeof(VertexData) * m.data.vertices.size());
	VertexData* vptr = nullptr;
	m.vb->Map(0, nullptr, reinterpret_cast<void**>(&vptr));
	std::memcpy(vptr, m.data.vertices.data(), sizeof(VertexData) * m.data.vertices.size());
	m.vb->Unmap(0, nullptr);

	m.vbv.BufferLocation = m.vb->GetGPUVirtualAddress();
	m.vbv.SizeInBytes = UINT(sizeof(VertexData) * m.data.vertices.size());
	m.vbv.StrideInBytes = sizeof(VertexData);
	return m;
}

static void DrawModel(
	ID3D12GraphicsCommandList* cmd,
	const GfxModel& model,
	ID3D12Resource* /*materialCB_unused*/, GPUMaterial* /*mat_unused*/,
	ID3D12Resource* /*transformCB_unused*/, TransformationMatrix* /*wvp_unused*/,
	const Matrix4x4& view, const Matrix4x4& proj,
	const Vector3& pos, const Vector3& scale, const Vector3& rot,
	const Vector4& rgba,
	D3D12_GPU_DESCRIPTOR_HANDLE textureHandle,
	ID3D12Resource* lightCB)
{
	// 1) このDraw用のオフセットを確保
	size_t matOff = kMatSize * (gMaterialCursor++);
	size_t xformOff = kXformSize * (gXformCursor++);

	// 2) CPU側に書き込み（テンプレートをベースに色など上書き）
	auto* mat = reinterpret_cast<GPUMaterial*>(gMaterialMapped + matOff);
	*mat = gMaterialTemplate;         // enableLighting/uvTransform等の共通値
	mat->color = rgba;

	auto* xform = reinterpret_cast<TransformationMatrix*>(gXformMapped + xformOff);
	Matrix4x4 world = MakeAffineMatrix(scale, rot, pos);
	xform->World = world;
	xform->WVP = Multiply(Multiply(world, view), proj);
	xform->WorldInverseTranspose = Transpose(Inverse(world));

	// 3) 描画
	cmd->IASetVertexBuffers(0, 1, &model.vbv);
	cmd->SetGraphicsRootConstantBufferView(0, gMaterialCB->GetGPUVirtualAddress() + matOff);
	cmd->SetGraphicsRootConstantBufferView(1, gXformCB->GetGPUVirtualAddress() + xformOff);
	cmd->SetGraphicsRootDescriptorTable(2, textureHandle);
	cmd->SetGraphicsRootConstantBufferView(3, lightCB->GetGPUVirtualAddress());
	cmd->DrawInstanced(model.vertexCount, 1, 0, 0);
}


void DrawTileMapWithModel(
	ID3D12GraphicsCommandList* cmd,
	const GfxModel& blockModel,
	ID3D12Resource* materialCB, GPUMaterial* mat,
	ID3D12Resource* transformCB, TransformationMatrix* wvp,
	const Matrix4x4& view, const Matrix4x4& proj,
	D3D12_GPU_DESCRIPTOR_HANDLE textureHandle,
	ID3D12Resource* lightCB)
{
	for (int y = 0; y < MAP_H; ++y) {
		for (int x = 0; x < MAP_W; ++x) {
			if (gMap[y][x] == 1) {
				// ←★ ここで反転（配列の最下行が yWorld=0 になる）
				const int yWorld = MAP_H - 1 - y;

				Vector3 pos{ x * TILE + TILE * 0.5f,
							 yWorld * TILE + TILE * 0.5f,
							 0.0f };
				Vector3 scale{ TILE, TILE, 1.0f };
				Vector3 rot{ 0,0,0 };
				DrawModel(cmd, blockModel, materialCB, mat, transformCB, wvp,
					view, proj, pos, scale, rot,
					{ 0.25f,0.25f,0.28f,1.0f }, textureHandle, lightCB);
			}
		}
	}
}

void UpdateBoss(Boss& B, const Player& P, float dt)
{
	float dx = P.pos.x - B.pos.x;
	float dist = fabsf(dx);
	B.facingRight = (dx > 0);

	// ------------------------
	// 状態遷移（あなたのコード）
	// ------------------------
	if (B.attackCooldown > 0) {
		B.attackCooldown -= dt;
	}

	switch (B.state)
	{
	case Boss::State::Idle:
		if (dist < 200.0f) {
			B.state = Boss::State::Approach;
		}
		break;

	case Boss::State::Approach:
	{
		// プレイヤー方向へ常に移動（距離に関係なく止まらない）
		B.vel.x = (B.facingRight ? B.walkSpeed : -B.walkSpeed);

		// 攻撃距離に入ったら攻撃
		if (dist < B.attackRange && B.attackCooldown <= 0) {
			B.state = Boss::State::Attack;
			break;
		}

		// 突進距離に入ったらダッシュへ
		if (dist < B.dashRange && B.attackCooldown <= 0) {
			B.state = Boss::State::DashPrep;
			break;
		}

		break;
	}


	case Boss::State::Attack:
		B.vel.x = 0;
		B.attackCooldown = 1.2f;
		B.state = Boss::State::Idle;
		break;

	case Boss::State::DashPrep:
	{
		B.vel.x = 0;
		B.attackCooldown = 0.1f;

		float dx = P.pos.x - B.pos.x;

		// ★ ダッシュ前に向きを確実に更新
		B.facingRight = (dx > 0);

		// ★ プレイヤーまで一瞬で届く速度
		B.dashInstantSpeed = fabsf(dx) * 10.0f;

		B.state = Boss::State::Dash;
		break;
	}

	case Boss::State::Dash:
		// ① 一瞬でプレイヤーに突進する速度をセット
		B.vel.x = (B.facingRight ? B.dashInstantSpeed : -B.dashInstantSpeed);

		// ② 衝突判定（攻撃距離に入ったら終了）
		if (dist < B.attackRange)
		{
			B.attackCooldown = 5.0f;
			B.state = Boss::State::Idle;
			B.vel.x = 0;
			break;
		}

		// ③ 通り過ぎたら終了
		if (dist > B.dashRange * 2.0f)
		{
			B.vel.x = 0;
			B.state = Boss::State::Idle;
			break;
		}

		break;
	}

	// ------------------------
	// ★ 重力を追加
	// ------------------------
	const float gravity = -22.0f;
	B.vel.y += gravity * dt;

	// ------------------------
	// ★ 衝突（プレイヤーの簡易版）
	// ------------------------
	float newY = B.pos.y + B.vel.y * dt;

	// 下方向だけの判定でOK（ボスはジャンプしない）
	float bottom = newY;
	int tileY = (int)floor(bottom / TILE);

	float left = B.pos.x;
	float right = B.pos.x + B.sizeX;

	int tileXLeft = (int)floor(left / TILE);
	int tileXRight = (int)floor(right / TILE);

	bool grounded = false;

	for (int tx = tileXLeft; tx <= tileXRight; ++tx) {
		int mapY = MAP_H - 1 - tileY;
		if (IsSolidTileByIndex(tx, mapY)) {
			newY = (tileY + 1) * TILE;
			B.vel.y = 0;
			grounded = true;
			break;
		}
	}

	B.pos.y = newY;

	// ------------------------
	// X更新（あなたのコード）
	// ------------------------
	B.pos.x += B.vel.x * dt;
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
	// 裏面を表示する
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

	HRESULT result;

	// DepthStencilの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// DirectInputの初期化
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
	
	// 旧: ModelData modelData = LoadObjFile("resources", "plane.obj"); … は削除
	GfxModel gModelPlayer = CreateGfxModel(device, "resources", "player.obj");
	GfxModel gModelEnemy = CreateGfxModel(device, "resources", "enemy.obj");
	GfxModel gModelBullet = CreateGfxModel(device, "resources", "bullet.obj");
	GfxModel gModelBlock = CreateGfxModel(device, "resources", "block.obj");
	GfxModel gModelBG = CreateGfxModel(device, "resources", "plane.obj"); // ← 背景用
	GfxModel gModelLine = CreateGfxModel(device, "resources", "line.obj"); // ← 背景用



	// リソース作成
	std::vector<ComPtr<ID3D12Resource>> textureResources;
	std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> textureSrvHandles;

	// GPU上のマテリアルリソース一覧（マテリアル名で識別）
	std::unordered_map<std::string, ComPtr<ID3D12Resource>> materialResources;

	// CPU側のマテリアルポインタ一覧（ImGuiで編集用）
	std::unordered_map<std::string, Material*> materialDataList;

	// ===== たくさん入るCB（リングバッファ）を作ってマップ =====
	gMaterialCB = CreateBufferResource(device, kMatSize * kMaxDraws);
	gXformCB = CreateBufferResource(device, kXformSize * kMaxDraws);

	gMaterialCB->Map(0, nullptr, reinterpret_cast<void**>(&gMaterialMapped));
	gXformCB->Map(0, nullptr, reinterpret_cast<void**>(&gXformMapped));


	// 平行光源のバッファを作成し、CPU 側から書き込めるようにする
	ComPtr<ID3D12Resource> directionalLightResource =
		CreateBufferResource(device, AlignCBSize(sizeof(GPUDirectionalLight)));
	GPUDirectionalLight* directionalLightData = nullptr;
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
	directionalLightData->color = { 1,1,1,1 };
	Vector3 dir = Normalize({ -1,-1,0 });
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

	// --- これを WinMain の SRV ヒープ作成後あたりに追加 ---
	enum TexSlot : UINT {
		kTexPlayer = 1,
		kTexEnemy = 2,
		kTexBullet = 3,
		kTexBlock = 4,
		kTexSky = 5,  // ★追加
	};


	auto LoadTextureToSlot = [&](const char* path, UINT slot,
		ComPtr<ID3D12Resource>& outTex,
		D3D12_GPU_DESCRIPTOR_HANDLE& outHandle)
		{
			DirectX::ScratchImage img = LoadTexture(path);
			const DirectX::TexMetadata& meta = img.GetMetadata();
			outTex = CreateTextureResource(device, meta);
			UploadTextureData(outTex, img);

			D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
			srv.Format = meta.format;
			srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srv.Texture2D.MipLevels = UINT(meta.mipLevels);

			auto cpu = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, slot);
			device->CreateShaderResourceView(outTex.Get(), &srv, cpu);
			outHandle = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, slot);
		};

	// --- これまで使っていた uvChecker は不要なら消してOK ---
	ComPtr<ID3D12Resource> texPlayer, texEnemy, texBullet, texBlock,texSky;
	D3D12_GPU_DESCRIPTOR_HANDLE hPlayer{}, hEnemy{}, hBullet{}, hBlock{}, hSky{};

	LoadTextureToSlot("resources/player.png", kTexPlayer, texPlayer, hPlayer);
	LoadTextureToSlot("resources/enemy.png", kTexEnemy, texEnemy, hEnemy);
	LoadTextureToSlot("resources/bullet.png", kTexBullet, texBullet, hBullet);
	LoadTextureToSlot("resources/block.png", kTexBlock, texBlock, hBlock);
	LoadTextureToSlot("resources/skydome.png", kTexSky, texSky, hSky);

	int kTexWhite = 10;
	ComPtr<ID3D12Resource> texWhite;
	D3D12_GPU_DESCRIPTOR_HANDLE hWhite;
	LoadTextureToSlot("resources/white.png", kTexWhite, texWhite, hWhite);


	// モデルの種類を選択するための変数

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


			//ImGui::Begin("Window");

			ImGui::SetItemDefaultFocus(); // ←追加！

			/*
			// 光の設定
			if (imgui::collapsingheader("light")) {
				const char* lightingitems[] = { "none", "lambert", "halflambert" };
				int currentlighting = static_cast<int>(lightingmode);
				if (imgui::combo("lighting mode", &currentlighting, lightingitems, im_arraysize(lightingitems))) {
					lightingmode = static_cast<lightingmode>(currentlighting);
				}
				static vector3 lightdiredit = { directionallightdata->direction.x, directionallightdata->direction.y, directionallightdata->direction.z };
				if (imgui::dragfloat3("light dir", &lightdiredit.x, 0.01f, -1.0f, 1.0f)) {
					vector3 normdir = normalize(lightdiredit);
					directionallightdata->direction = { normdir.x, normdir.y, normdir.z, 0.0f };
				}
				imgui::dragfloat("light intensity", &directionallightdata->intensity, 0.01f, 0.0f, 10.0f);
				imgui::coloredit3("light color", &directionallightdata->color.x);
			}

			imgui::end();
			
			imgui::begin("game state");
			switch (gstate) {
			case gamestate::title:
				imgui::textunformatted("title");
				imgui::separator();
				imgui::textunformatted("[space] start");
				imgui::textunformatted("[esc]   back to title (anytime)");
				imgui::textunformatted("[arrows] move, [space] jump, [z] shoot");
				break;
			case gamestate::playing:
				imgui::textunformatted("playing");
				imgui::textunformatted("kill the red enemy with [z]. touch = game over");
				break;
			case gamestate::clear:
				imgui::textunformatted("clear!");
				imgui::textunformatted("[space] back to title");
				break;
			case gamestate::gameover:
				imgui::textunformatted("game over");
				imgui::textunformatted("[space] back to title");
				break;
			}
			imgui::end();*/

			gMaterialTemplate.enableLighting = static_cast<int32_t>(lightingMode);
			gMaterialTemplate.uvTransform = MakeIdentity4x4(); // 必要なら

			// --- 入力は1回だけ ---
			keyboard->Acquire();
			memcpy(keyPre, key, sizeof(key)); // 前フレームのコピー
			keyboard->GetDeviceState(sizeof(key), key);

			auto Pressed = [&](int dik) { return (key[dik] & 0x80) && !(keyPre[dik] & 0x80); };
			auto Down = [&](int dik) { return (key[dik] & 0x80); };

			// --- 固定Δtも1回だけ ---
			const float dt = 1.0f / 60.0f;

			// グローバル変数に追加
			Vector2 cameraCenter{ 0.0f, 0.0f }; // 初期値はプレイヤー開始位置など

			// --- カメラ（2D直交投影：少し“引く”）---
			Matrix4x4 viewMatrix{}, projectionMatrix{};
			{
				// プレイヤー位置を追う
				Vector3 eye = {
					gPlayer.pos.x - 6.0f,  // 少し左から横を見る
					gPlayer.pos.y + 2.0f,  // 上方向はほんの少し
					-15.0f                 // 斜め後方から
				};

				Vector3 target = {
					gPlayer.pos.x,
					gPlayer.pos.y + 1.0f,
					0.0f
				};

				Vector3 up = { 0,1,0 };

				viewMatrix = MakeLookAtMatrixLH(eye, target, up);

				// 視野角とアスペクト比
				float fov = 0.45f; // ちょうどよい広さ
				float aspect = float(kClientWidth) / float(kClientHeight);

				projectionMatrix = MakePerspectiveFovMatrix(
					fov,
					aspect,
					0.1f,   // near
					1000.0f // far
				);
			}




			// 共通ショートカット：ESCでタイトルへ
			if (Pressed(DIK_ESCAPE)) {
				gState = GameState::Title;
			}

			// ----------------- 状態更新 -----------------
			switch (gState) {
			case GameState::Title:
				if (Pressed(DIK_SPACE)) { ResetGame(); gState = GameState::Playing; }
				if (gState == GameState::Title) {
					// 左上に操作説明を表示
					ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_Always);
					ImGui::SetNextWindowBgAlpha(0.0f); // 背景透明
					ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
						ImGuiWindowFlags_NoInputs |
						ImGuiWindowFlags_AlwaysAutoResize;
					ImGui::Begin("TitleHelp", nullptr, flags);
					ImGui::Text("[SPACE] Start");
					ImGui::Text("[A][D] Move");
					ImGui::Text("[SPACE] Jump (Wall Jump / Double Jump)");
					ImGui::Text("[LSHIFT] Air Dash");
					ImGui::Text("[Z] Charge Shot");
					ImGui::End();
				}

				break;

			case GameState::Playing: {
				// 左上に操作説明を表示
				ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_Always);
				ImGui::SetNextWindowBgAlpha(0.0f); // 背景透明
				ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
					ImGuiWindowFlags_NoInputs |
					ImGuiWindowFlags_AlwaysAutoResize;
				ImGui::Begin("GameHelp", nullptr, flags);
				ImGui::Text("[A][D] Move");
				ImGui::Text("[SPACE] Jump (Wall Jump / Double Jump)");
				ImGui::Text("[LSHIFT] Air Dash");
				ImGui::Text("[Z] Charge Shot");
				ImGui::End();
				Player& P = gPlayer;
				Boss& B = gBoss;

				const float runSpeed = 6.0f;
				const float gravity = -22.0f;
				const float jumpVel = 10.0f;

				// ------------------------
				// 入力
				// ------------------------
				float moveX = 0.0f;
				if (Down(DIK_LEFT) || Down(DIK_A)) moveX -= 1.0f;
				if (Down(DIK_RIGHT) || Down(DIK_D)) moveX += 1.0f;

				if (moveX != 0.0f)
					P.facingRight = (moveX > 0);

				if (P.wallKickLock > 0.0f) {
					P.wallKickLock -= dt;
				}


				// ------------------------
				// ダッシュ中は横入力を無視
				// ------------------------
				if (!P.isDashing && P.wallKickLock <= 0.0f) {
					P.vel.x = moveX * runSpeed;
				}


				// ------------------------
				// 重力
				// ------------------------
				P.vel.y += gravity * dt;

				// ------------------------
				// 衝突判定で書き換えるので壁フラグは初期化
				// ------------------------
				P.onWallLeft = false;
				P.onWallRight = false;

				// ========================
				// X方向移動（ここはあなたの元コードと同じ）
				// ========================


				// 壁キック中は X衝突判定をスキップ
				bool skipWallCollision = (P.wallKickLock > 0.0f);

				float newX = P.pos.x + P.vel.x * dt;

				if (!skipWallCollision) {
					float bottom = P.pos.y + 0.1f;
					float top = P.pos.y + kPlayerSize.y - 0.1f;

					if (P.vel.x > 0.0f) {
						float right = newX + kPlayerSize.x;
						int tileX = (int)floor(right / TILE);
						int tileYBottom = (int)floor(bottom / TILE);
						int tileYTop = (int)floor(top / TILE);

						for (int ty = tileYBottom; ty <= tileYTop; ++ty) {
							int mapY = MAP_H - 1 - ty;
							if (IsSolidTileByIndex(tileX, mapY)) {
								newX = tileX * TILE - kPlayerSize.x;
								P.vel.x = 0.0f;
								P.onWallRight = true;
								break;
							}
						}
					} else if (P.vel.x < 0.0f) {
						float left = newX;
						int tileX = (int)floor(left / TILE);
						int tileYBottom = (int)floor(bottom / TILE);
						int tileYTop = (int)floor(top / TILE);

						for (int ty = tileYBottom; ty <= tileYTop; ++ty) {
							int mapY = MAP_H - 1 - ty;
							if (IsSolidTileByIndex(tileX, mapY)) {
								newX = (tileX + 1) * TILE;
								P.vel.x = 0.0f;
								P.onWallLeft = true;
								break;
							}
						}
					}
				}

				P.pos.x = newX;

				// ========================
				// Y方向移動（元コード + 改良）
				// ========================

				P.onGround = false;

				float newY = P.pos.y + P.vel.y * dt;
				{
					float left = P.pos.x + 0.1f;
					float right = P.pos.x + kPlayerSize.x - 0.1f;

					// 上昇中
					if (P.vel.y > 0.0f) {
						float top = newY + kPlayerSize.y;
						int tileY = (int)floor(top / TILE);
						int tileXLeft = (int)floor(left / TILE);
						int tileXRight = (int)floor(right / TILE);

						for (int tx = tileXLeft; tx <= tileXRight; ++tx) {
							int mapY = MAP_H - 1 - tileY;
							if (IsSolidTileByIndex(tx, mapY)) {
								newY = tileY * TILE - kPlayerSize.y;
								P.vel.y = 0.0f;
								break;
							}
						}
					} else { // 落下中
						float bottom = newY;
						int tileY = (int)floor(bottom / TILE);
						int tileXLeft = (int)floor(left / TILE);
						int tileXRight = (int)floor(right / TILE);

						for (int tx = tileXLeft; tx <= tileXRight; ++tx) {
							int mapY = MAP_H - 1 - tileY;
							if (IsSolidTileByIndex(tx, mapY)) {
								newY = (tileY + 1) * TILE;
								P.vel.y = 0.0f;
								P.onGround = true;
								break;
							}
						}
					}
				}

				// ------------------------
				// 着地した瞬間のリセット
				// ------------------------
				if (P.onGround) {
					P.canDoubleJump = true;
					P.canAirDash = true;
				}

				// ------------------------
				// ★ 空中ダッシュ（飛距離アップ版）
				// ------------------------
				if (!P.onGround && !P.isDashing && P.canAirDash && Pressed(DIK_LSHIFT)) {
					P.isDashing = true;
					P.dashTimer = P.dashTime;

					float dir = (moveX == 0 ? (P.facingRight ? 1 : -1) : moveX);
					P.vel.x = dir * P.dashSpeed;
					P.vel.y = 0.0f;

					P.canAirDash = false;
				}

				if (P.isDashing) {
					P.dashTimer -= dt;
					if (P.dashTimer <= 0) {
						P.isDashing = false;
					}
				}

				// ------------------------
				// ★ ジャンプ（通常 / 壁 / 二段）
				// ------------------------
				// 壁ジャンプ（反対方向固定）
				if (Pressed(DIK_SPACE)) {

					if (P.onGround) {
						P.vel.y = jumpVel;
						P.canDoubleJump = true;
					} else if (P.onWallLeft) {
						// 左壁 → 必ず右へ
						P.vel.x = +14.0f;
						P.vel.y = +12.0f;

						P.facingRight = true;
						P.canDoubleJump = true;

						P.wallKickLock = 0.15f;  // 横移動封印
						P.onWallLeft = P.onWallRight = false;
					} else if (P.onWallRight) {
						// 右壁 → 必ず左へ
						P.vel.x = -14.0f;
						P.vel.y = +12.0f;

						P.facingRight = false;
						P.canDoubleJump = true;

						P.wallKickLock = 0.15f;  // 横移動封印
						P.onWallLeft = P.onWallRight = false;
					} else if (P.canDoubleJump) {
						P.vel.y = jumpVel;
						P.canDoubleJump = false;
					}
				}


				// ------------------------
				// ★ 壁スライド処理（ジャンプ後が正しい）
				// ------------------------
				if (!P.onGround && (P.onWallLeft || P.onWallRight)) {
					if (P.vel.y < P.wallSlideSpeed) {
						P.vel.y = P.wallSlideSpeed;
					}
				}

				// ------------------------
				P.pos.y = newY;
				ConstrainToZ0(P.pos);


				// ========================
				// ★ チャージ処理（Z押しっぱ）
				// ========================
				if (Down(DIK_Z)) {

					// ★ 押し始めた瞬間だけ初期化
					if (!P.isCharging) {
						gChargeFx.timer = 0.0f;
						gChargeFx.lastSparkTime = 0.0f;
						gLightningList.clear();          // ← 前の稲妻を消す
					}

					P.isCharging = true;

					P.charge += dt / P.maxChargeTime;
					if (P.charge > 1.0f) P.charge = 1.0f;

					gChargeFx.active = true;
					gChargeFx.timer += dt;
					gChargeFx.scale = 0.3f + P.charge * 1.0f;

				} else {
					// Z離した瞬間：弾発射（ここは今までどおりでOK）
					if (P.isCharging) {
						gChargeFx.active = false;
						gChargeFx.timer = 0.0f;

						float front = (P.facingRight ? 1.0f : -1.0f);
						float offset = 0.6f;

						float cx = P.pos.x + kPlayerSize.x * 0.5f + front * offset;
						float cy = P.pos.y + kPlayerSize.y * 0.5f;

						float power = 0.4f + P.charge * 1.6f;
						float speed = 14.0f + P.charge * 10.0f;
						float damage = 3.0f + P.charge * 10.0f;

						Bullet b;
						b.size = { power, power };
						b.pos = { cx - b.size.x * 0.5f, cy - b.size.y * 0.5f, 0.0f };
						b.vel = { front * speed, 0.0f, 0.0f };
						b.damage = damage;
						b.life = 1.0f + P.charge * 0.8f;
						b.alive = true;
						gBullets.push_back(b);
					}

					P.isCharging = false;
					P.charge = 0.0f;
				}


				if (gPlayer.invincible) {
					gPlayer.invincibleTime -= dt;
					if (gPlayer.invincibleTime <= 0) {
						gPlayer.invincible = false;
					}
				}

				if (gPlayer.invincible) {
					gPlayer.invincibleTime -= dt;
					if (gPlayer.invincibleTime <= 0) {
						gPlayer.invincible = false;
					}
				}


				UpdateBoss(gBoss, gPlayer, dt);



				// 弾更新 & 敵ヒット
				for (auto& b : gBullets) {
					if (!b.alive) continue;
					b.pos.x += b.vel.x * dt;
					b.pos.y += b.vel.y * dt;
					b.life -= dt;
					if (b.life <= 0.0f) b.alive = false;

					AABB aB{ b.pos.x, b.pos.y, b.size.x, b.size.y };
					AABB aBoss{ gBoss.pos.x, gBoss.pos.y, gBoss.sizeX, gBoss.sizeY };

					if (gBoss.alive && Intersects(aB, aBoss)) {
						gBoss.hp -= b.damage;
						b.alive = false;

						if (gBoss.hp <= 0) {
							gBoss.alive = false;
							gState = GameState::Clear;
						}
					}


				}

				if (gBoss.alive && !gPlayer.invincible) {
					AABB aP{ gPlayer.pos.x, gPlayer.pos.y, kPlayerSize.x, kPlayerSize.y };
					AABB aB{ gBoss.pos.x,   gBoss.pos.y,   gBoss.sizeX,   gBoss.sizeY };

					if (Intersects(aP, aB)) {

						// --- ダメージ ---
						gPlayer.hp -= 20.0f;  // 好きな値に変更

						// --- 無敵化 ---
						gPlayer.invincible = true;
						gPlayer.invincibleTime = gPlayer.invincibleDuration;
						gPlayer.blinkTimer = gPlayer.invincibleDuration;

						// --- ノックバック ---
						if (gPlayer.pos.x < gBoss.pos.x) {
							gPlayer.vel.x = -gPlayer.knockbackPower;
						} else {
							gPlayer.vel.x = +gPlayer.knockbackPower;
						}
						gPlayer.vel.y = gPlayer.knockbackUp;

						// --- 死亡 ---
						if (gPlayer.hp <= 0) {
							gState = GameState::GameOver;
						}
					}
				}

			} break;


			case GameState::Clear:
				if (Pressed(DIK_SPACE)) { gState = GameState::Title; }
				break;

			case GameState::GameOver:
				if (Pressed(DIK_SPACE)) { gState = GameState::Title; }
				break;
			}


			auto ShowCenterText = [&](const char* text) {
				ImGui::SetNextWindowPos(ImVec2(kClientWidth * 0.5f, kClientHeight * 0.5f),
					ImGuiCond_Always, ImVec2(0.5f, 0.5f));
				ImGui::SetNextWindowBgAlpha(0.0f);
				ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
					ImGuiWindowFlags_NoInputs |
					ImGuiWindowFlags_AlwaysAutoResize;
				ImGui::Begin("CenterTextOverlay", nullptr, flags);
				ImGui::SetWindowFontScale(2.5f);     // 文字を大きく
				ImGui::TextUnformatted(text);
				ImGui::End();
				};

			// シーンごとの表示
			if (gState == GameState::Title)      ShowCenterText("TITLE");
			if (gState == GameState::Clear)      ShowCenterText("GAME CLEAR");
			if (gState == GameState::GameOver)   ShowCenterText("GAME OVER");

			Matrix4x4 uiView = MakeIdentity4x4();

			// 画面ピクセル用の平行投影
			Matrix4x4 uiProj = MakeOrthographicMatrix(
				0.0f,                    // left
				(float)kClientHeight,    // top
				(float)kClientWidth,     // right
				0.0f,                    // bottom
				0.0f, 1.0f               // near, far
			);

			// ===========================
			// ★ プレイヤーの点滅用スカラー
			// ===========================
			float playerBlink = 1.0f; // 通常はそのまま

			if (gPlayer.invincible) {
				float t = gPlayer.invincibleTime;

				// 0.16秒周期で ON / OFF
				if (fmod(t, 0.16f) < 0.08f) {
					playerBlink = 3.0f;   // 明るく
				} else {
					playerBlink = 0.3f;   // 暗く
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
			ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap.Get()};
			commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

			// 深度バッファのクリア
			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);
			commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			// ★ここでリセット（毎フレーム最初に一度だけ）
			gMaterialCursor = 0;
			gXformCursor = 0;

			// RootSignatureとPSOの設定
			commandList->SetGraphicsRootSignature(rootSignature.Get());
			commandList->SetPipelineState(graphicsPipelineState.Get());

			// 頂点バッファの設定
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);




			if (gState == GameState::Playing) {

				Vector3 bgScale{ (float)kClientWidth, (float)kClientHeight, 1.0f };
				Vector3 bgPos{
					kClientWidth * 0.5f,
					kClientHeight * 0.5f,
					1.0f          // ちょっと奥め（深度的に後ろ）
				};
				Vector3 bgRot{ 0,0,0 };

				DrawModel(
					commandList.Get(),
					gModelBG,                // ← plane.obj
					nullptr, nullptr, nullptr, nullptr,
					uiView, uiProj,          // ← 画面用の行列
					bgPos, bgScale, bgRot,
					{ 1,1,1,1 },               // 色は白（テクスチャそのまま）
					hSky,                    // ← 背景テクスチャ
					directionalLightResource.Get()
				);

				// --- タイル ---
				DrawTileMapWithModel(commandList.Get(), gModelBlock,
					nullptr, nullptr, nullptr, nullptr,
					viewMatrix, projectionMatrix,
					hBlock,   // ← block.png
					directionalLightResource.Get());


				// --- プレイヤー ---
				{
					Vector3 pos{ gPlayer.pos.x + kPlayerSize.x * 0.5f, gPlayer.pos.y + kPlayerSize.y * 0.5f, 0.0f };
					Vector3 scale{ kPlayerSize.x, kPlayerSize.y, 0.6f };
					Vector3 rot{ 0,0,0 };

					DrawModel(
						commandList.Get(),
						gModelPlayer,
						nullptr, nullptr, nullptr, nullptr,
						viewMatrix, projectionMatrix,
						pos, scale, rot,
						{ playerBlink, playerBlink, playerBlink, 1.0f }, // ←ここだけ変更
						hPlayer,
						directionalLightResource.Get()
					);
				}


				if (gBoss.alive) {
					Vector3 pos{
						gBoss.pos.x + gBoss.sizeX * 0.5f,
						gBoss.pos.y + gBoss.sizeY * 0.5f,
						0.0f
					};
					Vector3 scale{ gBoss.sizeX, gBoss.sizeY, 0.7f };
					Vector3 rot{ 0,0,0 };

					DrawModel(commandList.Get(), gModelEnemy,
						nullptr, nullptr, nullptr, nullptr,
						viewMatrix, projectionMatrix,
						pos, scale, rot,
						{ 1,1,1,1 }, hEnemy, directionalLightResource.Get());
				}

				// ===============================
// ★ チャージ中（Lightning生成部）
// ===============================
				// --- チャージ中の「帯電してる弾」エフェクト ---
				if (gChargeFx.active)
				{
					float front = (gPlayer.facingRight ? 1.0f : -1.0f);
					float offset = 0.6f;

					// 弾の中心と同じ位置
					float cx = gPlayer.pos.x + kPlayerSize.x * 0.5f + front * offset;
					float cy = gPlayer.pos.y + kPlayerSize.y * 0.5f;

					gChargeFx.timer += dt;

					// 弾のコアの大きさ（チャージに比例）
					float coreScale = 0.3f + gPlayer.charge * 1.3f;

					// ===== 稲妻（弾の周りにバチバチ）を生成 =====
					if (gChargeFx.timer - gChargeFx.lastSparkTime > 0.01f)
					{
						gChargeFx.lastSparkTime = gChargeFx.timer;

						// 1回につき何本出すか（チャージで増える）
						int boltCount = 4 + (int)(gPlayer.charge * 6.0f); // 4～10本くらい

						for (int i = 0; i < boltCount; i++)
						{
							Lightning L;
							L.active = true;
							L.time = 0.0f;
							L.life = 0.10f; // すぐ消える

							L.points.clear();

							// ランダムな向き（0～2π）
							float angle = ((rand() % 100) / 100.0f) * 6.28318f; // 2π

							// 弾の周りの半径（チャージでちょい増える）
							float innerR = coreScale * 0.6f;
							float boltLen = coreScale * (0.5f + ((rand() % 100) / 100.0f)); // 弾サイズに比例

							// 始点：弾の表面付近
							Vector3 p0{
								cx + cosf(angle) * innerR,
								cy + sinf(angle) * innerR,
								0.0f
							};

							// 終点：少し外側
							Vector3 p1{
								cx + cosf(angle) * (innerR + boltLen),
								cy + sinf(angle) * (innerR + boltLen),
								0.0f
							};

							L.points.push_back(p0);
							L.points.push_back(p1);

							gLightningList.push_back(L);
						}
					}

					// ===== 稲妻の描画 =====
					for (auto& L : gLightningList)
					{
						if (!L.active) continue;

						L.time += dt;
						if (L.time > L.life) {
							L.active = false;
							continue;
						}

						float a = 1.0f - (L.time / L.life); // フェードアウト

						if (L.points.size() < 2) continue;

						Vector3 p0 = L.points[0];
						Vector3 p1 = L.points[1];

						Vector3 mid{
							(p0.x + p1.x) * 0.5f,
							(p0.y + p1.y) * 0.5f,
							0.0f
						};

						float dx = p1.x - p0.x;
						float dy = p1.y - p0.y;
						float len = sqrtf(dx * dx + dy * dy);
						float angle = atan2f(dy, dx);

						// 細長い電撃：太さもチャージで微増
						float thickness = 0.03f + gPlayer.charge * 0.10f;

						DrawModel(
							commandList.Get(),
							gModelLine,  // 細い板モデル
							nullptr, nullptr, nullptr, nullptr,
							viewMatrix, projectionMatrix,
							mid,
							{ len, thickness, 0.2f },
							{ 0,0,angle },
			{
				0.4f + gPlayer.charge * 0.6f, // 青白く
				0.4f + gPlayer.charge * 0.6f,
				1.0f,
				a
			},
							hWhite,
							directionalLightResource.Get()
						);
					}

					// ===== 中心の「光る弾」本体描画 =====
					Vector3 chargePos{ cx, cy, 0.0f };
					Vector3 chargeScale{
						coreScale,
						coreScale,
						0.3f
					};

					DrawModel(
						commandList.Get(),
						gModelBullet, // 弾モデル（板でもOK）
						nullptr, nullptr, nullptr, nullptr,
						viewMatrix, projectionMatrix,
						chargePos,
						chargeScale,
						{ 0,0, gChargeFx.timer * 8.0f }, // くるくる回す
						{ 0.3f + gPlayer.charge, 0.3f + gPlayer.charge, 1.0f, 1.0f },
						hWhite,
						directionalLightResource.Get()
					);
				}


				// --- 弾 ---
				for (auto& b : gBullets) if (b.alive) {
					Vector3 pos{ b.pos.x + b.size.x * 0.5f, b.pos.y + b.size.y * 0.5f, 0.0f };
					Vector3 scale{ b.size.x, b.size.y, 0.4f };
					Vector3 rot{ 0,0,0 };
					DrawModel(commandList.Get(), gModelBullet,
						nullptr, nullptr, nullptr, nullptr,
						viewMatrix, projectionMatrix,
						pos, scale, rot,
						{ 1,1,1,1 }, hBullet, directionalLightResource.Get());

				}

				// ===============================
				// ★ プレイヤー HP バー（左上）
				// ===============================
				{
					// hpRate 計算
					float hpRateP = gPlayer.hp / gPlayer.maxHp;
					if (hpRateP < 0.0f) hpRateP = 0.0f;
					if (hpRateP > 1.0f) hpRateP = 1.0f;

					// サイズ
					const float fullW = 200.0f;
					const float fullH = 20.0f;

					// 左上
					const float marginX = 20.0f;
					const float marginY = 20.0f;

					float centerX = marginX + fullW * 0.5f;
					float centerY = marginY + fullH * 0.5f;

					//------------------------
					// ① 背景（黒）
					//------------------------
					{
						Vector3 scale{ fullW, fullH, 1.0f };
						Vector3 pos{ centerX, centerY, 0.0f };
						DrawModel(
							commandList.Get(),
							gModelBlock,
							nullptr, nullptr, nullptr, nullptr,
							uiView, uiProj,
							pos, scale, { 0,0,0 },
							{ 0.0f, 0.0f, 0.0f, 1.0f },   // 黒
							hWhite,
							directionalLightResource.Get()
						);
					}

					//------------------------
					// ② 本体（緑）
					//------------------------
					{
						float w = fullW * hpRateP;

						Vector3 scale{ w, fullH, 1.0f };
						Vector3 pos{
							marginX + w * 0.5f,
							centerY,
							0.0f
						};

						DrawModel(
							commandList.Get(),
							gModelBlock,
							nullptr, nullptr, nullptr, nullptr,
							uiView, uiProj,
							pos, scale, { 0,0,0 },
							{ 0.2f, 1.0f, 0.2f, 1.0f },   // 緑
							hWhite,
							directionalLightResource.Get()
						);
					}
				}

				if (gBoss.alive)
				{
					// hpRate 計算
					float hpRate = gBoss.hp / gBoss.maxHp;
					if (hpRate < 0.0f) hpRate = 0.0f;
					if (hpRate > 1.0f) hpRate = 1.0f;

					// サイズ
					const float fullW = 250.0f;
					const float fullH = 20.0f;

					// 右下に配置
					const float marginX = 20.0f;
					const float marginY = 20.0f;

					// 中心座標
					float centerX = (float)kClientWidth - (marginX + fullW * 0.5f);
					float centerY = (float)kClientHeight - (marginY + fullH * 0.5f);

					//------------------------
					// ① 背景（黒）
					//------------------------
					{
						Vector3 scale{ fullW, fullH, 1.0f };
						Vector3 pos{ centerX, centerY, 0.0f };

						DrawModel(
							commandList.Get(),
							gModelBlock,
							nullptr, nullptr, nullptr, nullptr,
							uiView, uiProj,
							pos, scale, { 0,0,0 },
							{ 0.0f, 0.0f, 0.0f, 1.0f },   // 黒
							hWhite,
							directionalLightResource.Get()
						);
					}

					//------------------------
					// ② 本体（赤）
					//------------------------
					{
						float w = fullW * hpRate;

						Vector3 scale{ w, fullH, 1.0f };
						Vector3 pos{
							centerX - (fullW - w) * 0.5f,
							centerY,
							0.0f
						};

						DrawModel(
							commandList.Get(),
							gModelBlock,
							nullptr, nullptr, nullptr, nullptr,
							uiView, uiProj,
							pos, scale, { 0,0,0 },
							{ 1.0f, 0.2f, 0.2f, 1.0f },   // 赤
							hWhite,
							directionalLightResource.Get()
						);
					}
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
			ID3D12CommandList* commandLists[] = { commandList.Get()};
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
	if (keyboard) {
		keyboard->Unacquire();
		keyboard->Release();
		keyboard = nullptr;
	}



	CloseWindow(hwnd);

	// ImGuiの終了処理
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	// COMの終了処理
	CoUninitialize();
	return 0;


}

