#pragma once
#include <Windows.h>
#include <wrl.h>
#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定
#include <dinput.h>

// 入力
class Input {
public:
	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

	// 初期化
	void Initialize(HINSTANCE hInstance, HWND hwnd);

	// 更新
	void Update();
private:
	// メンバ変数
	ComPtr<IDirectInputDevice8> keyboard; // キーボードデバイス

};