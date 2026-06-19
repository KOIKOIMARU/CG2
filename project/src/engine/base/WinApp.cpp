#include "engine/base/WinApp.h"

#ifdef USE_IMGUI
#include "imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
#endif

// ウィンドウプロシージャ
LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {

#ifdef USE_IMGUI
	// ImGuiのウィンドウプロシージャを呼び出す（処理したらここで終了）
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return 1; // LRESULTなので true より 1 の方が安全
	}
#endif

	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}


void WinApp::Initialize()
{
	CoInitializeEx(0, COINIT_MULTITHREADED);


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


	// ウィンドウサイズを表す構造体にクライアント領域を入れる
	RECT wrc = { 0, 0, kClientWidth, kClientHeight };
	const DWORD windowStyle =
		WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX;

	// クライアント領域を元に実際のサイズにwrcを変更してもらう
	AdjustWindowRect(&wrc, windowStyle, false);


	// ウィンドウの作成
	hwnd = CreateWindow(
		wc.lpszClassName, // ウィンドウクラス名
		L"CG2", // ウィンドウ名
		windowStyle, // ウィンドウスタイル
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

}

void WinApp::Finalize()
{

	CloseWindow(hwnd);

	// COMの終了処理
	CoUninitialize();
}

bool WinApp::ProcessMessage()
{
	MSG msg{};
	if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		// メッセージがあったら処理する
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// ウィンドウの×ボタンが押されたらtrueを返す
	if (msg.message == WM_QUIT) {
		return true;
	}

	return false;
}
