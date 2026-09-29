#include "Input.h"
#include <cassert>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")


// 初期化処理
void Input::Initialize(HINSTANCE hInstance, HWND hwnd)
{
	HRESULT result;

	// --- DriectX初期化処理 ---
	result = DirectInput8Create(hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput, nullptr);
	assert(SUCCEEDED(result));

	// キーボードデバイスの生成
	result = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, nullptr);
	assert(SUCCEEDED(result));

	// 入力データ形式のセット 
	result = keyboard->SetDataFormat(&c_dfDIKeyboard);
	assert(SUCCEEDED(result));

	// 排他制御レベルのセット 
	result = keyboard->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY); // DISCL_NONEXCLUSIVE を追加
	assert(SUCCEEDED(result));

	// マウスデバイスの生成
	result = directInput->CreateDevice(GUID_SysMouse, &mouseDevice, nullptr);
	assert(SUCCEEDED(result));

	// マウスの入力データ形式のセット
	result = mouseDevice->SetDataFormat(&c_dfDIMouse);
	assert(SUCCEEDED(result));

	// マウスの排他制御レベルのセット
	result = mouseDevice->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	assert(SUCCEEDED(result));

}

// 更新処理
void Input::Update()
{
	// 前回のキー入力を保存
	memcpy(keyPre, key, sizeof(key));
	// キーボード情報の取得
	keyboard->Acquire();
	keyboard->GetDeviceState(sizeof(key), key);
	
	// マウス情報の取得
	if (mouseDevice) {
		mouseDevice->Acquire();
		mouseDevice->GetDeviceState(sizeof(DIMOUSESTATE), &mouseState);
	}

}

// キーの押下チェック
bool Input::PushKey(BYTE keyNumber)
{
	// 指定キーを押していれば true を返す
	if (key[keyNumber]) {
		return true;
	}
	// そうでなければ false を返す
	return false;
}

// キーのトリガーチェック
bool Input::TriggerKey(BYTE keyNumber)
{
	// 今回押されている（key）、前回押されていない（!keyPre）true
	if (key[keyNumber] && !keyPre[keyNumber]) {
		return true;
	}
	return false;
}

bool Input::PushMouse(int buttonNumber)
{
	// マウスボタンの状態を判定して true / false を返す
	return (mouseState.rgbButtons[buttonNumber] & 0x80) != 0;
}