#include "DirectXCommon.h"
#include "Logger.h"
#include "StringUtility.h"
#include <cassert>
#include <format>
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#include "externals/imgui/imgui_impl_win32.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/DirectXTex/d3dx12.h"
#include <thread>

using namespace Microsoft::WRL;

// ===== 初期化処理 =====
void DirectXCommon::Initialize(WinApp* winApp)
{

	// NULL検出
	assert(winApp);

	// メンバ変数に記録
	this->winApp = winApp;

	// FPS固定初期化
	InitializeFixFPS();

	// FPS固定更新
	UpdateFixFPS();

	// デバイスの生成
	DeviceInitialize();

	// コマンド関連の生成
	InitializeCommand();

	// スワップチェーンの生成
	InitializeSwapChain();

	// 深度バッファの生成
	InitializeDepthBuffer();

	// 各種ディスクリプタヒープの生成
	InitializeDescriptorHeaps();

	// レンダーターゲットビューの初期化
	InitializeRenderTargetView();

	// 深度ステンシルビューの初期化
	InitializeDepthStencilView();

	// フェンスの生成
	InitializeFence();

	// ビューポート矩形の初期化
	InitializeViewport();

	// シザリング矩形の初期化
	InitializeScissorRect();

	// DXCコンパイラの生成
	InitializeDXCCompiler();

	// ImGuiの初期化 
	InitializeImGui();

}

// ===== 描画前処理 =====
void DirectXCommon::PreDraw()
{
	// --- バックバッファの番号取得 ---
	UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();
	// --- リソースバリアで書き込み可能に ---
	// TransitionBarrierの設定
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = swapChainResources[backBufferIndex].Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	commandList->ResourceBarrier(1, &barrier);
	// --- 描画先のRTVとDSVを指定 ---
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);
	// --- 画面全体の色をクリア ---
	// float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };   // 青
	float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f }; // 黒
	commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);
	// --- 画面全体の深度をクリア ---
	commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	// --- SRV用のデスクリプタヒープを指定 ---
	ID3D12DescriptorHeap* heaps[] = { srvDescriptorHeap.Get() };
	commandList->SetDescriptorHeaps(1, heaps);
	// --- ビューポートの設定 ---
	commandList->RSSetViewports(1, &viewport);
	// --- シザー矩形の設定 ---
	commandList->RSSetScissorRects(1, &scissorRect);

}

// ===== 描画後処理 =====
void DirectXCommon::PostDraw()
{
	HRESULT hr;

	// --- バックバッファの番号取得 ---
	UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

	// --- リソースバリアを表示状態に変更 ---
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = swapChainResources[backBufferIndex].Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	commandList->ResourceBarrier(1, &barrier);

	// --- グラフィックスコマンドをクローズ ---
	hr = commandList->Close();
	assert(SUCCEEDED(hr));

	// --- GPUコマンドの実行 ---
	ID3D12CommandList* commandLists[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(1, commandLists);

	// --- 画面のフリップ（表示） ---
	hr = swapChain->Present(1, 0);
	assert(SUCCEEDED(hr));

	// --- Fenceの値を更新してシグナルを送る ---
	fenceValue++;
	commandQueue->Signal(fence.Get(), fenceValue);

	// --- コマンド完了待ち ---
	if (fence->GetCompletedValue() < fenceValue) {
		fence->SetEventOnCompletion(fenceValue, fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);
	}

	// FPS固定
	UpdateFixFPS();

	// --- 次のフレームのためのリセット --- 
	hr = commandAllocator->Reset();
	assert(SUCCEEDED(hr));
	hr = commandList->Reset(commandAllocator.Get(), nullptr);
	assert(SUCCEEDED(hr));
}

// ===== FPS固定更新 =====
void DirectXCommon::InitializeFixFPS() {
	// 現在時間を記録する
	reference_ = std::chrono::steady_clock::now();
}

// ===== FPS固定更新 =====
void DirectXCommon::UpdateFixFPS() {
	// 1/60秒ぴったりの時間
	const std::chrono::microseconds kMinTime(uint64_t(1000000.0f / 60.0f));
	// 1/60秒よりわずかに短い時間
	const std::chrono::microseconds kMinCheckTime(uint64_t(1000000.0f / 65.0f));

	// 現在時間を取得する
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	// 前回記録からの経過時間を取得する
	std::chrono::microseconds elapsed =
		std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);

	// 1/60秒(よりわずかに短い時間)経っていない場合
	if (elapsed < kMinCheckTime) {
		// 1/60秒経過するまで微小なスリープを繰り返す
		while (std::chrono::steady_clock::now() - reference_ < kMinTime) {
			// 1マイクロ秒スリープ
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
	}
	// 現在の時間を記録する
	reference_ = std::chrono::steady_clock::now();
}

// ===== デバイスの初期化 =====
void DirectXCommon::DeviceInitialize()
{
	HRESULT hr;

	// --- デバッグレイヤーをオンに ---
#ifdef _DEBUG
	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		debugController->EnableDebugLayer();
		debugController->SetEnableGPUBasedValidation(TRUE);
	}

#endif

	// --- DXGIファクトリーの生成 ---
	hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));

	assert(SUCCEEDED(hr));


	// --- アダプターの列挙 ---
	Microsoft::WRL::ComPtr<IDXGIAdapter4> useAdapter = nullptr;

	for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i,
		DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) !=
		DXGI_ERROR_NOT_FOUND; ++i) {


		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = useAdapter->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr));

		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			Logger::Log(StringUtility::ConvertString(std::format(L"Use Adapter : {}\n", adapterDesc.Description)));
			break;
		}
		useAdapter.Reset();
	}

	assert(useAdapter != nullptr);


	// --- デバイス生成 ---
	D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0 };
	const char* featureLevelStrings[] = { "12.2", "12.1", "12.0" };
	for (size_t i = 0; i < _countof(featureLevels); ++i) {
		hr = D3D12CreateDevice(useAdapter.Get(), featureLevels[i], IID_PPV_ARGS(&device));
		if (SUCCEEDED(hr)) {
			Logger::Log(std::format("FeatureLevel : {}\n", featureLevelStrings[i]));
			break;
		}
	}
	assert(device != nullptr);
	Logger::Log("Complete create D3D12Device!!!\n"); // 初期化完了のログをだす 


	// --- エラー時にブレークを発生させる設定 ---
#ifdef _DEBUG
	ID3D12InfoQueue* infoQueue = nullptr;
	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);

		D3D12_MESSAGE_ID denyIds[] = {
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};

		D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		infoQueue->PushStorageFilter(&filter);


		infoQueue->Release();

	}
#endif

}

// ===== コマンド関連の初期化 =====
void DirectXCommon::InitializeCommand()
{
	HRESULT hr;
	// --- コマンドアロケータ ---
	hr = device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&commandAllocator)
	);
	assert(SUCCEEDED(hr));

	// --- コマンドリスト ---
	hr = device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		commandAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(&commandList)
	);
	assert(SUCCEEDED(hr));

	// --- コマンドキュー ---
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	hr = device->CreateCommandQueue(
		&commandQueueDesc,
		IID_PPV_ARGS(&commandQueue)
	);
	assert(SUCCEEDED(hr));

}

// ==== スワップチェーンの生成 =====
void DirectXCommon::InitializeSwapChain() {
	HRESULT hr;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = WinApp::kClientWidth;
	swapChainDesc.Height = WinApp::kClientHeight;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	hr = dxgiFactory->CreateSwapChainForHwnd(
		commandQueue.Get(),
		winApp->GetHwnd(),
		&swapChainDesc,
		nullptr,
		nullptr,
		reinterpret_cast<IDXGISwapChain1**>(swapChain.GetAddressOf())
	);
	assert(SUCCEEDED(hr));

}

// ===== 深度バッファの生成 =====
void DirectXCommon::InitializeDepthBuffer() {
	HRESULT hr;
	// --- 深度バッファリソース設定 ---
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = WinApp::kClientWidth; // Textureの幅
	resourceDesc.Height = WinApp::kClientHeight; // Textureの高さ
	resourceDesc.MipLevels = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // DepthStencil用フォーマット
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	// --- 利用するヒープの設定 ---
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // VRAM上に作成

	// --- 深度値のクリア設定 ---
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f; // 最大値でクリア
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// --- 深度バッファ生成 / リソースの生成 ---
	hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, // 深度書き込み状態
		&depthClearValue,
		IID_PPV_ARGS(&depthStencilResource) // メンバ変数に直接入れる
	);
	assert(SUCCEEDED(hr));
}

// ===== 各種ディスクリプタヒープの生成 =====
void DirectXCommon::InitializeDescriptorHeaps() {

	// --- 各種デスクリプタサイズを取得しておく ---
	descriptorSizeRTV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	descriptorSizeDSV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	// --- RTV用のヒープ生成 ---
	rtvDescriptorHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);

	// --- SRV用のヒープ生成 ---
	srvDescriptorHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);

	// --- DSV用のヒープ生成 ---
	dsvDescriptorHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
}

// ==================================================================
// DescriptorHeapを生成する関数 
// ==================================================================
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>
DirectXCommon::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible)
{
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
	descriptorHeapDesc.Type = heapType;
	descriptorHeapDesc.NumDescriptors = numDescriptors;
	descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;

	HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
	assert(SUCCEEDED(hr));

	return descriptorHeap;
}

// =============================
// DescriptorHandleを取得する関数
// ============================= 
// CPUHandle
D3D12_CPU_DESCRIPTOR_HANDLE
DirectXCommon::GetCPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
	UINT descriptorSize, UINT index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += descriptorSize * index;
	return handleCPU;
}
// GPUHandle
D3D12_GPU_DESCRIPTOR_HANDLE
DirectXCommon::GetGPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
	UINT descriptorSize, UINT index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += descriptorSize * index;
	return handleGPU;
}


// =====  レンダーターゲットビューの初期化 =====
void DirectXCommon::InitializeRenderTargetView() {
	HRESULT hr;

	// --- SwapChainからResourceを引っ張って来る ---
	hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
	assert(SUCCEEDED(hr));
	hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
	assert(SUCCEEDED(hr));

	// --- RTVの設定 ---
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	// --- RTVヒープの先頭ハンドルを取得 ---
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	// --- レンダーターゲットビューを生成する ---
	rtvHandles[0] = rtvStartHandle;
	device->CreateRenderTargetView(swapChainResources[0].Get(), &rtvDesc, rtvHandles[0]);

	rtvHandles[1].ptr = rtvHandles[0].ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	device->CreateRenderTargetView(swapChainResources[1].Get(), &rtvDesc, rtvHandles[1]);

}

// ===== SRVのCPUデスクリプタハンドルを取得する =====
D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVCPUDescriptorHandle(UINT index) const {
	return GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, index);
}

// ===== SRVのGPUデスクリプタハンドルを取得する =====
D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVGPUDescriptorHandle(UINT index) const {
	return GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, index);
}

// ===== 深度ステンシルビューの初期化 =====
void DirectXCommon::InitializeDepthStencilView() {
	// --- DSVの設定 ---
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

	// --- DSVをデスクリプタヒープの先頭につくる ---
	device->CreateDepthStencilView(
		depthStencilResource.Get(),
		&dsvDesc,
		dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart()
	);
}

// ==== フェンスの生成 =====
void DirectXCommon::InitializeFence() {
	HRESULT hr;
	fenceValue = 0;
	hr = device->CreateFence(
		fenceValue,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&fence)
	);
	assert(SUCCEEDED(hr));

	fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent != nullptr);
}

// ==== ビューポート矩形の初期化　====
void DirectXCommon::InitializeViewport() {
	viewport.Width = WinApp::kClientWidth;
	viewport.Height = WinApp::kClientHeight;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}

// ==== シザリング矩形の初期化　====
void DirectXCommon::InitializeScissorRect() {
	scissorRect.left = 0;
	scissorRect.right = WinApp::kClientWidth;
	scissorRect.top = 0;
	scissorRect.bottom = WinApp::kClientHeight;
}

// ==== DXCコンパイラの生成 ====
void DirectXCommon::InitializeDXCCompiler() {
	HRESULT hr;
	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
	assert(SUCCEEDED(hr));

	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
	assert(SUCCEEDED(hr));

	hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
	assert(SUCCEEDED(hr));
}

// ===== ImGuiの初期化 =====
void DirectXCommon::InitializeImGui() {
#ifdef USE_IMGUI
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(winApp->GetHwnd());
	ImGui_ImplDX12_Init(device.Get(),
		2,
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		srvDescriptorHeap.Get(),
		srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	ImGuiIO& io = ImGui::GetIO();

	// 日本語を使えるようにする
	io.Fonts->AddFontFromFileTTF(
		"C:/Windows/Fonts/meiryo.ttc",          // フォントファイル
		18.0f,                                   // 文字の大きさ
		nullptr,
		io.Fonts->GetGlyphRangesJapanese());     // 日本語の文字を含める

	io.Fonts->Build();
#endif
}

// ===== シェーダーのコンパイル =====
Microsoft::WRL::ComPtr<IDxcBlob> DirectXCommon::CompileShader(
	const std::wstring& filePath,
	const wchar_t* profile)
{
	// HLSLファイルを読む
	Logger::Log(StringUtility::ConvertString(std::format(L"Begin CompileShader, path:{}, profile:{}\n", filePath, profile)));
	Microsoft::WRL::ComPtr<IDxcBlobEncoding> shaderSource;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	assert(SUCCEEDED(hr));

	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;

	// Compileする 
	LPCWSTR arguments[] = {
		filePath.c_str(),
		L"-E", L"main",
		L"-T", profile,
		L"-Zi",
		L"-Qembed_debug",
		L"-Od",
		L"-Zpr"
	};

	Microsoft::WRL::ComPtr<IDxcResult> shaderResult;
	hr = dxcCompiler->Compile(
		&shaderSourceBuffer,
		arguments,
		_countof(arguments),
		includeHandler.Get(),
		IID_PPV_ARGS(&shaderResult)
	);
	assert(SUCCEEDED(hr));

	// 警告・エラーが出ていないか確認する
	Microsoft::WRL::ComPtr<IDxcBlobUtf8> shaderError;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Logger::Log(shaderError->GetStringPointer());
		assert(false);
	}

	// Compile結果を受け取って返す
	Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));

	Logger::Log(StringUtility::ConvertString(std::format(L"Compile Succeeded, path:{}, profile:{}\n", filePath, profile)));

	return shaderBlob;
}

// ===== バッファリソースの生成 =====
Microsoft::WRL::ComPtr<ID3D12Resource>
DirectXCommon::CreateBufferResource(size_t sizeInBytes) {
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeInBytes;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	Microsoft::WRL::ComPtr<ID3D12Resource> resource;
	HRESULT hr = device->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&resource)
	);
	assert(SUCCEEDED(hr));

	return resource;
}

// ===== テクスチャリソースの生成 =====
Microsoft::WRL::ComPtr<ID3D12Resource>
DirectXCommon::CreateTextureResource(const DirectX::TexMetadata& metadata) {
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width);
	resourceDesc.Height = UINT(metadata.height);
	resourceDesc.MipLevels = UINT16(metadata.mipLevels);
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);
	resourceDesc.Format = metadata.format;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	Microsoft::WRL::ComPtr<ID3D12Resource> resource;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&resource)
	);
	assert(SUCCEEDED(hr));

	return resource;
}

// ===== テクスチャデータの転送 =====
ID3D12Resource* DirectXCommon::UploadTextureData(
	ID3D12Resource* texture,
	const DirectX::ScratchImage& mipImages)
{
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	DirectX::PrepareUpload(device.Get(), mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);

	uint64_t intermediateSize = GetRequiredIntermediateSize(texture, 0, UINT(subresources.size()));
	ID3D12Resource* intermediateResource = CreateBufferResource(intermediateSize).Detach();

	UpdateSubresources(commandList.Get(), texture, intermediateResource, 0, 0, UINT(subresources.size()), subresources.data());

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	commandList->ResourceBarrier(1, &barrier);

	return intermediateResource;
}

// ===== テクスチャファイルの読み込み =====
DirectX::ScratchImage DirectXCommon::LoadTexture(const std::string& filePath) {
	DirectX::ScratchImage image{};
	std::wstring filePathW = StringUtility::ConvertString(filePath);
	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	DirectX::ScratchImage mipChain{};
	hr = DirectX::GenerateMipMaps(
		image.GetImages(), image.GetImageCount(), image.GetMetadata(),
		DirectX::TEX_FILTER_DEFAULT, 0, mipChain);
	assert(SUCCEEDED(hr));
	return image;
}