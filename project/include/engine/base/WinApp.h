#pragma once
#include <Windows.h>
#include <cstdint>

// Win32ウィンドウの生成、メッセージ処理、表示モード切り替えを担当する。
class WinApp
{
public:
	// Win32から届いたイベントを、作成時に関連付けたWinAppへ転送する。
	static LRESULT CALLBACK WindowProc(
		HWND hwnd,
		UINT message,
		WPARAM wparam,
		LPARAM lparam);

	// 描画基盤が基準解像度として使用するクライアント領域。
	static constexpr int32_t kClientWidth = 1280;
	static constexpr int32_t kClientHeight = 720;

	void Initialize();
	void Finalize();
	// 保留中のWindowsメッセージをすべて処理し、終了要求の有無を返す。
	bool ProcessMessage();

	// 現在のモニター全体を使うボーダーレス表示と通常表示を切り替える。
	void ToggleFullscreen();
	bool IsFullscreen() const { return isFullscreen_; }

	// HWNDとHINSTANCEは他のWin32／DirectX初期化処理へ貸し出す。
	HWND GetHwnd() const { return hwnd_; }
	HINSTANCE GetHInstance() const { return windowClass_.hInstance; }

private:
	// このインスタンスが所有するトップレベルウィンドウ。
	HWND hwnd_ = nullptr;
	// RegisterClassへ渡した定義。終了時の登録解除にも使用する。
	WNDCLASS windowClass_{};
	// フルスクリーンへ入る直前のスタイルと位置。通常表示へ戻すために保存する。
	DWORD windowedStyle_ = 0;
	WINDOWPLACEMENT windowedPlacement_{ sizeof(WINDOWPLACEMENT) };
	// trueの間は枠を外して現在モニター全体へウィンドウを広げている。
	bool isFullscreen_ = false;
};
