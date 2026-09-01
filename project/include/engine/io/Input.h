#pragma once
#include <Windows.h>
#include <wrl.h>
#define DIRECTINPUT_VERSION 0x0800 // 使用するDirectInput APIのバージョン
#include <dinput.h>
#include "engine/base/Math.h"
#include "engine/base/WinApp.h"

// DirectInputのキーボード状態とWin32のマウス位置をフレーム単位で保持する。
// TriggerKeyは押した瞬間だけ、PushKeyは押している間継続してtrueを返す。
class Input {
public:
	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

	// ウィンドウを借用し、キーボードデバイスを初期化する。
	void Initialize(WinApp* winApp);

	// 前フレームの状態を保存してから最新入力を取得する。
	void Update();

	// 指定キーが現在押されている間trueを返す。
	bool PushKey(BYTE keyNumber);

	// 指定キーがこのフレームに押された瞬間だけtrueを返す。
	bool TriggerKey(BYTE keyNumber);

	const Math::Vector2& GetMousePosition() const { return mousePosition_; }

private:
	ComPtr<IDirectInputDevice8> keyboard; // 入力を取得するDirectInputキーボード
	ComPtr<IDirectInput8> directInput;    // DirectInputデバイスの生成元
	BYTE key[256] = {};                   // 現在フレームの全キー状態
	BYTE keyPre[256] = {};                // 直前フレームの全キー状態
	Math::Vector2 mousePosition_{ 0.0f, 0.0f }; // クライアント領域内のマウス座標
	WinApp* winApp = nullptr;             // 座標変換に使うウィンドウの借用先
};
