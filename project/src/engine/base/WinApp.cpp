#include "engine/base/WinApp.h"

#ifdef USE_IMGUI
#include "imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam);
#endif

LRESULT CALLBACK WinApp::WindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam)
{
    // CreateWindowへ渡したWinAppをHWNDへ記録し、以後のイベントから取得する。
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCT*>(lparam);
        SetWindowLongPtr(
            hwnd,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    auto* winApp = reinterpret_cast<WinApp*>(
        GetWindowLongPtr(hwnd, GWLP_USERDATA));

    // 表示切り替えはImGuiの入力キャプチャ中でも常に利用できるよう先に処理する。
    const bool isFirstKeyDown =
        (static_cast<uint64_t>(lparam) & (1ull << 30)) == 0;
    const bool isF11 =
        message == WM_KEYDOWN && wparam == VK_F11 && isFirstKeyDown;
    const bool isAltEnter =
        message == WM_SYSKEYDOWN &&
        wparam == VK_RETURN &&
        (static_cast<uint64_t>(lparam) & (1ull << 29)) != 0 &&
        isFirstKeyDown;
    if ((isF11 || isAltEnter) && winApp) {
        winApp->ToggleFullscreen();
        return 0;
    }

#ifdef USE_IMGUI
    // ImGuiが使用したマウス・キーボードイベントは、通常の処理へ重複して渡さない。
    if (ImGui_ImplWin32_WndProcHandler(hwnd, message, wparam, lparam)) {
        return 1;
    }
#endif

    switch (message) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_NCDESTROY:
        SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        break;
    }

    return DefWindowProc(hwnd, message, wparam, lparam);
}

void WinApp::Initialize()
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    windowClass_.lpfnWndProc = WindowProc;
    windowClass_.lpszClassName = L"CG2WindowClass";
    windowClass_.hInstance = GetModuleHandle(nullptr);
    windowClass_.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&windowClass_);

    // 固定サイズの通常ウィンドウを作り、F11時には同じHWNDの枠だけを外す。
    windowedStyle_ =
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX;
    RECT windowRect{ 0, 0, kClientWidth, kClientHeight };
    AdjustWindowRect(&windowRect, windowedStyle_, FALSE);

    hwnd_ = CreateWindow(
        windowClass_.lpszClassName,
        L"CG2",
        windowedStyle_,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        windowClass_.hInstance,
        this);

    ShowWindow(hwnd_, SW_SHOW);
}

void WinApp::Finalize()
{
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (windowClass_.lpszClassName && windowClass_.hInstance) {
        UnregisterClass(
            windowClass_.lpszClassName,
            windowClass_.hInstance);
    }
    CoUninitialize();
}

bool WinApp::ProcessMessage()
{
    MSG message{};
    // 1フレームでキューを空にし、入力やリサイズが描画に遅れて反映されるのを防ぐ。
    while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            return true;
        }
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
    return false;
}

void WinApp::ToggleFullscreen()
{
    if (!hwnd_) {
        return;
    }

    if (!isFullscreen_) {
        MONITORINFO monitorInfo{ sizeof(MONITORINFO) };
        const HMONITOR monitor =
            MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
        if (!GetMonitorInfo(monitor, &monitorInfo)) {
            return;
        }

        // 通常表示へ戻すため、枠を変更する前の状態を保存する。
        windowedStyle_ = static_cast<DWORD>(
            GetWindowLongPtr(hwnd_, GWL_STYLE));
        windowedPlacement_.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement(hwnd_, &windowedPlacement_);

        SetWindowLongPtr(
            hwnd_,
            GWL_STYLE,
            static_cast<LONG_PTR>(
                windowedStyle_ & ~WS_OVERLAPPEDWINDOW));
        SetWindowPos(
            hwnd_,
            HWND_TOP,
            monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.top,
            monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
            SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        isFullscreen_ = true;
        return;
    }

    SetWindowLongPtr(
        hwnd_,
        GWL_STYLE,
        static_cast<LONG_PTR>(windowedStyle_));
    SetWindowPlacement(hwnd_, &windowedPlacement_);
    SetWindowPos(
        hwnd_,
        nullptr,
        0,
        0,
        0,
        0,
        SWP_NOMOVE |
        SWP_NOSIZE |
        SWP_NOZORDER |
        SWP_NOOWNERZORDER |
        SWP_FRAMECHANGED);
    isFullscreen_ = false;
}
