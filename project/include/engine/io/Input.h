#pragma once
#include <Windows.h>
#include <wrl.h>
#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定
#include <dinput.h>
#include "engine/base/Math.h"
#include "engine/base/WinApp.h"
#ifdef _DEBUG
#include <array>
#endif

// 入力
class Input {
public:
	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

	// 初期化
	void Initialize(WinApp* winApp);

	// 更新
	void Update();

	/// <summary>
	/// キーの押下をチェック
	/// </summary>
	/// <param name="keyNumber"></param>
	/// <returns>押されているか</returns>
	bool PushKey(BYTE keyNumber);

	/// <summary>
	/// キーのトリガーをチェック
	/// </summary>
	/// <param name="keyNumber"></param>
	/// <returns>トリガーか</returns>
	bool TriggerKey(BYTE keyNumber);

	const Math::Vector2& GetMousePosition() const { return mousePosition_; }
#ifdef _DEBUG
    // 通しテスト専用。OSへ入力は送らず、通常のPushKey/TriggerKey経路を検証する。
    void SetTestFrame(const std::array<BYTE, 256>& keys, const Math::Vector2& mouse) {
        memcpy(key, keys.data(), sizeof(key));
        mousePosition_ = mouse;
    }
#endif

private:
	// メンバ変数
	ComPtr<IDirectInputDevice8> keyboard; // キーボードデバイス

	//  DirectInputインターフェース
	ComPtr<IDirectInput8> directInput;

	// 全キーの状態
	BYTE key[256] = {};
	BYTE keyPre[256] = {};
	Math::Vector2 mousePosition_{ 0.0f, 0.0f };

	//WindowsAPI
	WinApp* winApp = nullptr;
};
