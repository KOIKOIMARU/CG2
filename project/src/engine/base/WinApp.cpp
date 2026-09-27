#include "engine/base/WinApp.h"

#ifdef USE_IMGUI
#include "imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
#endif

// ウィンドウプロシージャ
LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {

    auto* app = reinterpret_cast<WinApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        app = static_cast<WinApp*>(reinterpret_cast<CREATESTRUCT*>(lparam)->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (app && (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) &&
        (wparam == VK_F11 || (wparam == VK_RETURN && (lparam & (1LL << 29)) != 0))) {
        if ((lparam & (1LL << 30)) == 0) { app->toggleFullscreenRequested_ = true; }
        return 0;
    }
    if (msg == WM_SYSCHAR && wparam == VK_RETURN) { return 0; }
    if (msg == WM_GETMINMAXINFO) {
        auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
        info->ptMinTrackSize = { 656, 399 };
        return 0;
    }

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
	const DWORD windowStyle = WS_OVERLAPPEDWINDOW;

	// クライアント領域を元に実際のサイズにwrcを変更してもらう
	AdjustWindowRect(&wrc, windowStyle, false);


	// ウィンドウの作成
	hwnd = CreateWindow(
		wc.lpszClassName, // ウィンドウクラス名
		L"SKYBREAK", // 仮のゲーム名
		windowStyle, // ウィンドウスタイル
		CW_USEDEFAULT, // 表示X座標(Windowsに任せる
		CW_USEDEFAULT, // 表示Y座標
		wrc.right - wrc.left, // ウィンドウ横幅
		wrc.bottom - wrc.top, // ウィンドウ縦幅
		nullptr, // 親ウィンドウハンドル
		nullptr, // メニューハンドル
		wc.hInstance, // インスタンスハンドル
		this); // ウィンドウ処理から状態へアクセスする

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
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) { return true; }
		// メッセージがあったら処理する
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

    if (toggleFullscreenRequested_) {
        toggleFullscreenRequested_ = false;
        ToggleFullscreen();
    }

	return false;
}

void WinApp::ToggleFullscreen()
{
    if (!hwnd) { return; }
    if (!isFullscreen_) {
        MONITORINFO monitor{ sizeof(MONITORINFO) };
        if (!GetWindowPlacement(hwnd, &windowedPlacement_) ||
            !GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor)) { return; }
        SetWindowLongPtr(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(hwnd, HWND_TOP, monitor.rcMonitor.left, monitor.rcMonitor.top,
            monitor.rcMonitor.right - monitor.rcMonitor.left,
            monitor.rcMonitor.bottom - monitor.rcMonitor.top, SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
        isFullscreen_ = true;
    } else {
        SetWindowLongPtr(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
        SetWindowPlacement(hwnd, &windowedPlacement_);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
            SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
        isFullscreen_ = false;
    }
}
