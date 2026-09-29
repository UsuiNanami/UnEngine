#include "WinApp.h"
#include <cstdint>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
#endif

// ウィンドウプロ―ジャー 
LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
#ifdef USE_IMGUI
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}
#endif
	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

// 初期化処理 
void WinApp::Initialize() {
	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);

	// WNDCLASS wc{};
	wc.lpfnWndProc = WindowProc;
	wc.lpszClassName = L"CG2WindowClass"; // ウィンドウクラス名
	wc.hInstance = GetModuleHandle(nullptr);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	RegisterClass(&wc);

	RECT wrc = { 0, 0, kClientWidth, kClientHeight };
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	hwnd = CreateWindow(
		wc.lpszClassName,       // 利用するクラス名
		L"CG2",                 // タイトルバーの文字
		WS_OVERLAPPEDWINDOW,    // よく見るウィンドウスタイル
		CW_USEDEFAULT,          // 表示X座標（Windowsに任せる）
		CW_USEDEFAULT,          // 表示Y座標（WindowsOSに任せる）　
		wrc.right - wrc.left,   // ウィンドウ横幅
		wrc.bottom - wrc.top,   // ウィンドウ縦幅
		nullptr,                // 親ウィンドウハンドル
		nullptr,                // メニューハンドル
		wc.hInstance,           // インスタンスハンドル
		nullptr);               // オプション 

	// ウィンドウを表示させる 
	ShowWindow(hwnd, SW_SHOW);

}

// メッセージ処理
bool WinApp::ProcessMessage() {
	MSG msg{};

	if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// 終了メッセージが来たら true を返す
	if (msg.message == WM_QUIT) {
		return true;
	}

	return false;
}

// 終了処理
void WinApp::Finalize() {
	CloseWindow(hwnd);
	CoUninitialize();
}