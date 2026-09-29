#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include "WinApp.h"
#include <array>
#include <dxcapi.h>
#include <string>
#include "externals/DirectXTex/DirectXTex.h"
#include <chrono>

// DirectX基盤
class DirectXCommon
{
public: // メンバ関数
	// ===== 初期化 =====
	void Initialize(WinApp* winApp);

	// ===== 描画前処理 =====
	void PreDraw();
	// ===== 描画後処理 =====
	void PostDraw();

	// ===== 外部からヒープやサイズを取得するためのゲッター =====
	ID3D12Device* GetDevice() const { return device.Get(); }
	ID3D12GraphicsCommandList* GetCommandList() const { return commandList.Get(); }

	ID3D12CommandQueue* GetCommandQueue() const { return commandQueue.Get(); }
	ID3D12CommandAllocator* GetCommandAllocator() const { return commandAllocator.Get(); }
	ID3D12Fence* GetFence() const { return fence.Get(); }
	HANDLE GetFenceEvent() const { return fenceEvent; }

	ID3D12DescriptorHeap* GetRtvDescriptorHeap() const { return rtvDescriptorHeap.Get(); }
	ID3D12DescriptorHeap* GetSrvDescriptorHeap() const { return srvDescriptorHeap.Get(); }
	ID3D12DescriptorHeap* GetDsvDescriptorHeap() const { return dsvDescriptorHeap.Get(); }
	ID3D12Resource* GetDepthStencilResource() const { return depthStencilResource.Get(); }

	UINT GetDescriptorSizeRTV() const { return descriptorSizeRTV; }
	UINT GetDescriptorSizeSRV() const { return descriptorSizeSRV; }
	UINT GetDescriptorSizeDSV() const { return descriptorSizeDSV; }

	// ===== デスクリプタハンドルを取得する =====
	D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUDescriptorHandle(UINT index) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUDescriptorHandle(UINT index) const;

	// ===== シェーダーのコンパイル =====
	Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
		const std::wstring& filePath,
		const wchar_t* profile);

	// ===== バッファリソースの生成 =====
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);

	// ===== テクスチャリソースの生成 =====
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata);

	// ===== テクスチャデータの転送 =====
	[[nodiscard]]
	ID3D12Resource* UploadTextureData(
		ID3D12Resource* texture,
		const DirectX::ScratchImage& mipImages);

	// ===== テクスチャファイルの読み込み =====
	static DirectX::ScratchImage LoadTexture(const std::string& filePath);

private:
	// ===== デバイスの初期化 =====
	void DeviceInitialize();
	// --- DirectX12デバイス ---
	Microsoft::WRL::ComPtr<ID3D12Device> device;
	// --- DXGIファクトリ ---
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory;

	// ===== コマンド関連の初期化 ===== 
	void InitializeCommand();
	// --- コマンドアロケータ ---
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
	// --- コマンドリスト ---
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
	// --- コマンドキュー ---
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;

	// ===== スワップチェーンの生成 =====
	void InitializeSwapChain();
	// --- スワップチェーン生成の設定 ---
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain;
	// --- WinAppのポインタ ---
	WinApp* winApp = nullptr;

	// ===== 深度バッファの生成 =====
	void InitializeDepthBuffer();
	// --- 深度バッファリソース設定 ---
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource;

	// ===== 各種ディスクリプタヒープの生成 ===== 
	void InitializeDescriptorHeaps();
	// デスクリプタヒープを生成する関数 
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
		D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);
	// --- RTV用ディスクリプタヒープ ---
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;
	// --- SRV用ディスクリプタヒープ ---
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap;
	// --- DSV用ディスクリプタヒープ ---
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap;
	// --- 各種デスクリプタサイズ ---
	UINT descriptorSizeRTV = 0;
	UINT descriptorSizeSRV = 0;
	UINT descriptorSizeDSV = 0;

	// ===== レンダーターゲットビューの初期化 =====
	void InitializeRenderTargetView();
	// --- CPUデスクリプタハンドルを取得する ---
	static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(
		const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
		UINT descriptorSize, UINT index);
	// --- GPUデスクリプタハンドルを取得する ---
	static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(
		const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
		UINT descriptorSize, UINT index);
	// --- スワップチェーンリソース ---
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> swapChainResources;
	// RTVのハンドルをメンバ変数として保持する
	std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 2> rtvHandles;

	// ===== 深度ステンシルビューの初期化 =====
	void InitializeDepthStencilView();

	// ==== フェンスの生成 =====
	void InitializeFence();
	// --- フェンス ---
	Microsoft::WRL::ComPtr<ID3D12Fence> fence;
	uint64_t fenceValue = 0;
	// フェンスイベントをメンバ変数として持たせる
	HANDLE fenceEvent = nullptr;

	// ===== ビューポート矩形の初期化 =====
	void InitializeViewport();
	// --- ビューポート矩形 ---
	D3D12_VIEWPORT viewport{};

	// ===== シザリング矩形の初期化 =====
	void InitializeScissorRect();
	// --- シザリング矩形 ---
	D3D12_RECT scissorRect{};

	// ===== DXCコンパイラの生成 =====
	void InitializeDXCCompiler();
	// --- DXCコンパイラ関連 ---
	Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils;
	Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler;

	// ===== ImGuiの初期化 =====
	void InitializeImGui();

	// ===== FPS固定初期化 =====
	void InitializeFixFPS();
	// ===== FPS固定更新 =====
	void UpdateFixFPS();
	// 記録時間(FPS固定用)
	std::chrono::steady_clock::time_point reference_;

};