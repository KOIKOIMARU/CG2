#include "engine/io/Input.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>


#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

//using namespace Microsoft::WRL;

void Input::Initialize(WinApp* winApp) {
	if (!winApp) {
		throw std::invalid_argument("Input::Initialize requires WinApp");
	}

	this->winApp = winApp;
	HRESULT result;

	result = DirectInput8Create(
		winApp->GetHInstance(),
		DIRECTINPUT_VERSION, IID_IDirectInput8,
		reinterpret_cast<void**>(directInput.ReleaseAndGetAddressOf()), nullptr);
	if (FAILED(result)) {
		throw std::runtime_error("DirectInput8Create failed");
	}

	result = directInput->CreateDevice(
		GUID_SysKeyboard,
		keyboard.ReleaseAndGetAddressOf(),
		nullptr);
	if (FAILED(result)) {
		throw std::runtime_error("DirectInput keyboard creation failed");
	}

	result = keyboard->SetDataFormat(&c_dfDIKeyboard);
	if (FAILED(result)) {
		throw std::runtime_error("DirectInput keyboard format setup failed");
	}

	result = keyboard->SetCooperativeLevel(
		winApp->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	if (FAILED(result)) {
		throw std::runtime_error("DirectInput cooperative level setup failed");
	}
}

void Input::Update() {
	std::memcpy(keyPre, key, sizeof(key));

	// フォーカスを失った間は全キーを離した状態にする。
	// 復帰時に古い押下状態が残り、キャラクターが動き続けることを防ぐ。
	std::memset(key, 0, sizeof(key));
	if (keyboard) {
		HRESULT result = keyboard->GetDeviceState(sizeof(key), key);
		if (result == DIERR_INPUTLOST || result == DIERR_NOTACQUIRED) {
			result = keyboard->Acquire();
			if (SUCCEEDED(result)) {
				result = keyboard->GetDeviceState(sizeof(key), key);
			}
		}
		if (FAILED(result)) {
			std::memset(key, 0, sizeof(key));
		}
	}

	POINT cursorPosition{};
	if (winApp && GetCursorPos(&cursorPosition)) {
		ScreenToClient(winApp->GetHwnd(), &cursorPosition);
		RECT clientRect{};
		GetClientRect(winApp->GetHwnd(), &clientRect);
		const float clientWidth = static_cast<float>(
			(std::max)(clientRect.right - clientRect.left, 1L));
		const float clientHeight = static_cast<float>(
			(std::max)(clientRect.bottom - clientRect.top, 1L));
		mousePosition_.x = std::clamp(
			static_cast<float>(cursorPosition.x),
			0.0f,
			clientWidth);
		mousePosition_.y = std::clamp(
			static_cast<float>(cursorPosition.y),
			0.0f,
			clientHeight);
	}
}

bool Input::PushKey(BYTE keyNumber)
{
	return (key[keyNumber] & 0x80) != 0;
}

bool Input::TriggerKey(BYTE keyNumber)
{
	return (key[keyNumber] & 0x80) != 0 && (keyPre[keyNumber] & 0x80) == 0;
}
