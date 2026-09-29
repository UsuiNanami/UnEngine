#pragma once
#include <Windows.h>
#include <cstdint>

// WindowsAPI
class WinApp
{
public: // 定数
	// クライアント領域のサイズ
	static const int32_t kClientWidth = 1280;
	static const int32_t kClientHeight = 720;
public:
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

public:
	// 初期化
	void Initialize();

	// メッセージ処理
	bool ProcessMessage();

	// 終了処理
	void Finalize();

	// getter
	HWND GetHwnd() const { return hwnd; }
	HINSTANCE GetHInstance() const { return wc.hInstance; }

private:
	// ウィンドウクラスの設定
	WNDCLASS wc{};
	// ウィンドウハンドル
	HWND hwnd = nullptr;


};
