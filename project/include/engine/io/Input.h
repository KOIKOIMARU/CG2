#pragma once
#include <Windows.h>
#include <wrl.h>
#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定
#include <dinput.h>
#include "engine/base/WinApp.h"

// 入力
class Input {
public:
	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

	// 初期化
	void Initialize(WinApp* winApp);

	// 更新
	void Update();

	bool PushKey(BYTE keyNumber) const;
	bool TriggerKey(BYTE keyNumber) const;


private:
	// メンバ変数
	ComPtr<IDirectInputDevice8> keyboard; // キーボードデバイス

	//  DirectInputインターフェース
	ComPtr<IDirectInput8> directInput;

	// 全キーの状態
	BYTE key[256] = {};
	BYTE keyPre[256] = {};

	//WindowsAPI
	WinApp* winApp = nullptr;
};