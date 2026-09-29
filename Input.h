#pragma once
#include <Windows.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

// 入力クラス
class Input
{
public:
	// namespace省略
	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public: // メンバ関数
	// 初期化
	void Initialize(HINSTANCE hInstance, HWND hwnd);
	// 更新
	void Update();
	// 押されているか
	bool PushKey(BYTE keyNumber);
	// 押した瞬間
	bool TriggerKey(BYTE keyNumber);

	// マウスの指定ボタンが押されているか
	bool PushMouse(int buttonNumber);

	// マウスデバイス用ゲッター
	IDirectInputDevice8* GetMouseDevice() const { return mouseDevice.Get(); }

private: // メンバ変数
	// DirectInputのインスタンス
	ComPtr<IDirectInput8> directInput = nullptr;
	// キーボードデバイス
	ComPtr<IDirectInputDevice8> keyboard = nullptr;
	// マウスデバイス
	ComPtr<IDirectInputDevice8> mouseDevice = nullptr;

	// 全キーの状態
	BYTE key[256] = {};
	// 前フレームの全キーの状態
	BYTE keyPre[256] = {};
	// マウスの状態を保存する変数
	DIMOUSESTATE mouseState = {};

};