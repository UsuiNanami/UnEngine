#include <Windows.h>    // Windows APIを使うためのヘッダ
#include <cstdint>      // int32_tなどの整数型を使うためのヘッダ
#include <string>       // std::string 文字列を扱うためのヘッダ
#include <format>       // std::format 文字列のフォーマットを行うためのヘッダ
#include <filesystem>   // std::filesystem ファイルやフォルダを扱うためのヘッダ
#include <fstream>      // std::ifstream ファイルの入出力を扱うためのヘッダ
#include <chrono>       // std::chrono 時間を扱うためのヘッダ
#include <d3d12.h>      // DirectX 12を使うためのヘッダ
#include <dxgi1_6.h>    // DirectX Graphics Infrastructure (DXGI) を使うためのヘッダ
#include <cassert>      // assert マクロを使うためのヘッダ
#include <dbghelp.h>    // デバッグヘルプライブラリを使うためのヘッダ
#include <strsafe.h>    // StringCchPrintfWを使うために必要
#include <dxgidebug.h>  // DXGIのデバッグレイヤーを使うためのヘッダ
#include <dxcapi.h>     // DirectX Shader Compilerを使うためのヘッダ
#include "MyMath.h"     // 自作の数学ライブラリを使うためのヘッダ
#include "externals/DirectXTex/DirectXTex.h" // テクスチャを読み込むためのライブラリを使うためのヘッダ
#include "externals/DirectXTex/d3dx12.h"     // d3dx12ヘッダー
#include <fstream>      // ifstream 用
#include <sstream>      // istringstream 用（後で行解析に使う）
#include <wrl.h>        // Microsoft::WRL::ComPtrを使うため
#include <xaudio2.h>    // XAudio2を使うため  
#include <fstream>
#define DIRECTINPUT_VERSION 0x0800  // DirectInputのバージョン指定 
#include <dinput.h>
#include "DebugCamera.h" 
#include <vector>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"             // ImGuiを使うためのヘッダ  
#include "externals/imgui/imgui_impl_dx12.h"   // ImGuiのDirectX 12用の実装を使うためのヘッダ
#include "externals/imgui/imgui_impl_win32.h"  // ImGuiのWindows用の実装を使うためのヘッダ
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

// *libはヘッダには書いてはいけない！
#pragma comment(lib, "d3d12.lib")      // DirectX 12のライブラリをリンクするための指示
#pragma comment(lib, "dxgi.lib")       // DXGIのライブラリをリンクするための指示
#pragma comment(lib, "Dbghelp.lib")    // デバッグヘルプライブラリをリンクするための指示
#pragma comment(lib, "dxguid.lib")     // DXGIのGUIDをリンクするための指示
#pragma comment(lib, "dxcompiler.lib") // DirectX Shader Compilerをリンクするための指示
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "dinput8.lib")

// ==============================
// CrashHandlerの登録(ExportDump)
// ==============================
static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception) {

	SYSTEMTIME time;
	GetLocalTime(&time);
	wchar_t filePath[MAX_PATH] = { 0 };
	CreateDirectory(L"./Dumps", nullptr);
	StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d%02d.dmp",
		time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
	HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
	DWORD processId = GetCurrentProcessId();
	DWORD threadId = GetCurrentThreadId();
	MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{ 0 };
	minidumpInformation.ThreadId = threadId;
	minidumpInformation.ExceptionPointers = exception;
	minidumpInformation.ClientPointers = TRUE;
	MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle,
		MiniDumpNormal, &minidumpInformation, nullptr, nullptr);

	return EXCEPTION_EXECUTE_HANDLER;
}


// ==================================
// string->wstring 文字コード変換の関数
// ==================================
std::wstring ConvertString(const std::string& str) {
	if (str.empty()) {
		return std::wstring();
	}

	auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), NULL, 0);
	if (sizeNeeded == 0) {
		return std::wstring();
	}
	std::wstring result(sizeNeeded, 0);
	MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);
	return result;
}


// ==================================　
// wstring->string 文字コード変換の関数
// ==================================
std::string ConvertString(const std::wstring& str) {
	if (str.empty()) {
		return std::string();
	}

	auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
	if (sizeNeeded == 0) {
		return std::string();
	}
	std::string result(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), sizeNeeded, NULL, NULL);
	return result;
}


// ===========================
// 出力ウィンドウに文字を出す関数  
// ===========================
void Log(const std::string& message, std::ofstream& os) {
	// Visual Studioの「出力」ウィンドウに出す
	OutputDebugStringA(message.c_str());
	// ファイルに書き出す
	if (os.is_open()) {
		os << message << std::endl;
	}
}


// ==============================================
// ログファイルを生成して書き込み用ストリームを返す関数 
// ============================================== 
std::ofstream InitializeLogFile() {
	std::filesystem::create_directory("logs");
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
		nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
	std::chrono::zoned_time localTime{ std::chrono::current_zone(), nowSeconds };
	std::string dateString = std::format("{:%Y%m%d_%H%M%S}", localTime);
	std::string logFilePath = std::string("logs/") + dateString + ".log";
	std::ofstream logStream(logFilePath);
	return logStream;
}


// =============================
// DescriptorHandleを取得する関数 
// =============================
// CPUHandle
D3D12_CPU_DESCRIPTOR_HANDLE
GetCPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
	uint32_t descriptorSize, uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += descriptorSize * index;
	return handleCPU;
}
// GPUHandle
D3D12_GPU_DESCRIPTOR_HANDLE
GetGPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
	uint32_t descriptorSize, uint32_t index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += descriptorSize * index;
	return handleGPU;
}


// ==================================================================
// Windowsのウィンドウプロシージャ（ウィンドウに来たメッセージを処理する関数）
// ==================================================================
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {

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

// ==================================================================
// CompileShader関数 
// ==================================================================
Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
	const std::wstring& filePath,
	const wchar_t* profile,
	const Microsoft::WRL::ComPtr<IDxcUtils>& dxcUtils,
	const Microsoft::WRL::ComPtr<IDxcCompiler3>& dxcCompiler,
	const Microsoft::WRL::ComPtr<IDxcIncludeHandler>& includeHandler,
	std::ofstream& logFile)
{
	// 1. HLSLファイルを読む
	Log(ConvertString(std::format(L"Begin CompileShader, path:{}, profile:{}\n", filePath, profile)), logFile);
	Microsoft::WRL::ComPtr<IDxcBlobEncoding> shaderSource;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	assert(SUCCEEDED(hr));
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;
	// 2. Compileする
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
	// 3. 警告・エラーが出ていないか確認する
	Microsoft::WRL::ComPtr<IDxcBlobUtf8> shaderError;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Log(shaderError->GetStringPointer(), logFile);
		assert(false);
	}
	// 4. Compile結果を受け取って返す
	Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));

	Log(ConvertString(std::format(L"Compile Succeeded, path:{}, profile:{}\n", filePath, profile)), logFile);

	return shaderBlob;
}

// ==================================================================
// BufferResourceを生成する関数 
// ==================================================================
Microsoft::WRL::ComPtr<ID3D12Resource>
CreateBufferResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device, SIZE_T sizeInBytes) {
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


// ====================================================
// Transform変数をまとめる構造体
// ====================================================
struct Transform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

Transform transform{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
Transform cameraTransform{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -10.0f} }; // カメラのTransform

Transform transformSprite{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} }; // Sprite用のTransform

// ==================================================================
// DescriptorHeapを生成する関数
// ==================================================================
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>
CreateDescriptorHeap(const Microsoft::WRL::ComPtr<ID3D12Device>& device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible)
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


// ====================================================
// Textureデータを読み込む関数
// ====================================================
DirectX::ScratchImage LoadTexture(const std::string& filePath) {
	DirectX::ScratchImage image{};
	std::wstring filePathW = ConvertString(filePath);
	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));
	DirectX::ScratchImage mipChain{};
	hr = DirectX::GenerateMipMaps(
		image.GetImages(), image.GetImageCount(), image.GetMetadata(),
		DirectX::TEX_FILTER_DEFAULT, 0, mipChain);
	assert(SUCCEEDED(hr));
	return image;
}



// ====================================================
// TextureResourceを生成する関数
// ====================================================
Microsoft::WRL::ComPtr<ID3D12Resource>CreateTextureResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device, const DirectX::TexMetadata& metadata)
{
	// 1.metadataを基にResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width);
	resourceDesc.Height = UINT(metadata.height);
	resourceDesc.MipLevels = UINT16(metadata.mipLevels);
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);
	resourceDesc.Format = metadata.format;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);
	// 2.利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	// 3. Resourceの生成 
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


// ====================================================
// TextureResourceにデータを転送する関数
// ====================================================
[[nodiscard]]
ID3D12Resource* UploadTextureData(
	ID3D12Resource* texture,
	const DirectX::ScratchImage& mipImages,
	ID3D12Device* device,
	ID3D12GraphicsCommandList* commandList)
{
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	DirectX::PrepareUpload(device, mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);

	uint64_t intermediateSize = GetRequiredIntermediateSize(texture, 0, UINT(subresources.size()));
	ID3D12Resource* intermediateResource = CreateBufferResource(device, intermediateSize).Detach();

	// データ書き込み＆転送コマンドを積む  
	UpdateSubresources(commandList, texture, intermediateResource, 0, 0, UINT(subresources.size()), subresources.data());

	// Textureへの転送後は利用できるよう、D3D12_RESOURCE_STATE_COPY_DESTからD3D12_RESOURCE_STATE_GENERIC_READへResourceStateを変更する
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


// ====================================================
// DepthStencilTextureResourceを生成する関数
// ====================================================
Microsoft::WRL::ComPtr<ID3D12Resource>CreateDepthStencilTextureResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device, int32_t width, int32_t height)
{
	// Resource設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width; // Textureの幅
	resourceDesc.Height = height; // Textureの高さ
	resourceDesc.MipLevels = 1;

	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // DepthStencil用フォーマット
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	// Heap設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // VRAM上に作成

	// 深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f; // 最大値でクリア
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// Resource生成
	Microsoft::WRL::ComPtr<ID3D12Resource> resource;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, // 深度書き込み状態
		&depthClearValue,
		IID_PPV_ARGS(&resource)
	);
	assert(SUCCEEDED(hr));

	return resource;
}


// Lighting方式の定義
enum LightingType {
	Lighting_None = 0,       // Lightingなし 
	Lighting_Lambert = 1,    // Lambert
	Lighting_HalfLambert = 2 // Half Lambert
};

// Material構造体の定義 
struct Material {
	Vector4 color;
	int32_t lightingType;  // enableLighting から lightingType に変更
	float padding[3];      // Alignmentを満たすための余白 (12バイト)
	Matrix4x4 uvTransform; // UV変換行列
};

// TransformationMatrix構造体の定義
struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
};

// DirectionalLight構造体の定義
struct DirectionalLight {
	Vector4 color;      // ライトの色
	Vector3 direction;  // ライトの向き（単位ベクトル）
	float intensity;    // 輝度（明るさ） 
};


// UVTransform用の変数定義
Transform uvTransformSprite{
	{ 1.0f, 1.0f, 1.0f }, // スケール
	{ 0.0f, 0.0f, 0.0f }, // 回転
	{ 0.0f, 0.0f, 0.0f }  // 平行移動
};


// MaterialData構造体の定義
struct MaterialData
{
	std::string textureFilePath;
};

// ====================================================
// mtlファイルを読み込む関数
// ====================================================
MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename)
{
	// 1. 中で必要となる変数の宣言
	MaterialData materialData; // 構築するMaterialData
	std::string line; // ファイルから読み込む1行分

	// 2. ファイルを開く
	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
	assert(file.is_open()); // ファイルが開けなかった場合はアサートで止める

	// 3. 実際にファイルを読み、MaterialDataを構築していく
	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		// identifierに応じた処理
		if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;

			// 連結してファイルパスにする
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}

	// 4. MaterialDataを返す 
	return materialData;
}

// メッシュ単体のデータを保持する構造体
struct MeshData {
	std::vector<VertexData> vertices;
	MaterialData material;

	// DirectX 12 描画用リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
};

// モデル全体の構造体（複数のメッシュをまとめる） 
struct ModelData {
	std::vector<MeshData> meshes;
};

// ====================================================
// OBJファイルを読み込む関数
// ====================================================
ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename)
{
	// 1. 必要な変数を宣言
	ModelData modelData;              // 読み込んだ結果を格納する構造体
	std::vector<Vector4> positions;   // v（位置）データ
	std::vector<Vector3> normals;     // vn（法線）データ
	std::vector<Vector2> texcoords;   // vt（テクスチャ座標）データ
	std::string line;                 // ファイルから読み込む1行分

	// 2. ファイルを開く
	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open());

	MeshData currentMesh; // 現在読み込み中のメッシュ

	// 3. ファイルを読み込み、ModelDataを構築
	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier; // 先頭の識別子を読む

		// identifier に応じた処理
		if (identifier == "v") { // 位置情報
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.w = 1.0f;
			positions.push_back(position);
		}
		else if (identifier == "vt") { // テクスチャ座標
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoord.y = 1.0f - texcoord.y;
			texcoords.push_back(texcoord);
		}
		else if (identifier == "vn") { // 法線情報
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normals.push_back(normal);
		}
		else if (identifier == "usemtl") { // マテリアルの切り替え
			// 既に頂点データが入っている場合は、前のメッシュとして登録してリセット
			if (!currentMesh.vertices.empty()) {
				modelData.meshes.push_back(currentMesh);
				currentMesh = MeshData();
			}

			std::string materialName;
			s >> materialName;
			// 必要に応じて mtl から読み込んだマテリアル情報を割当
		}
		else if (identifier == "f") { // 面情報

			VertexData triangle[3]; // 三角形の頂点データを格納する配列

			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
				std::string vertexDefinition;
				s >> vertexDefinition;

				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3];
				for (int32_t element = 0; element < 3; ++element) {
					std::string index;
					std::getline(v, index, '/'); // "/"で区切ってインデックスを読む
					elementIndices[element] = std::stoi(index);
				}

				// インデックスから実際のデータを取得
				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcoord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];

				// 左手座標系から右手座標系に変換するために、X軸の符号を反転
				position.x *= -1.0f;
				normal.x *= -1.0f;

				triangle[faceVertex] = { position, texcoord, normal };
			}

			// 現在のメッシュに頂点を追加
            currentMesh.vertices.push_back(triangle[2]);
            currentMesh.vertices.push_back(triangle[1]);
            currentMesh.vertices.push_back(triangle[0]);
		}
	}

	// 最後のメッシュを追加
	if (!currentMesh.vertices.empty()) {
		modelData.meshes.push_back(currentMesh);
	}

	return modelData;
}


// =====================
// WAVEファイル用構造体
// =====================
struct ChunkHeader { // チャンク毎のID 
	char id[4];      // チャンクサイズ
	int32_t size;
};

// RIFFヘッダチャンク
struct RiffHeader {
	ChunkHeader chunk; // RIFF
	char type[4];      // WAVE
};

// FMTヘッダチャンク
struct FormatChunk {
	ChunkHeader chunk; // "fmt"
	WAVEFORMATEX fmt;  // 波形フォーマット
};

// =====================
// 音声データ
// =====================
struct SoundData
{
	// 波形フォーマット
	WAVEFORMATEX wfex;
	// バッファの先頭アドレス
	BYTE* pBuffer;
	// バッファのサイズ
	unsigned int bufferSize;
};

// =====================
// 音声データ読み込み関数
// =====================
SoundData SoundLoadWave(const char* filename) {
	// 1. ファイルを開く
	std::ifstream file;
	file.open(filename, std::ios_base::binary);
	assert(file.is_open());

	// 2. RIFFヘッダーの読み込み
	RiffHeader riff;
	file.read((char*)&riff, sizeof(riff));
	// ファイルがRIFFかチェック
	if (strncmp(riff.chunk.id, "RIFF", 4) != 0) {
		assert(false);
	}
	// タイプがWAVEかチェック　
	if (strncmp(riff.type, "WAVE", 4) != 0) {
		assert(false);
	}

	// 3. Formatチャンクの読み込み
	FormatChunk format = {};
	file.read((char*)&format, sizeof(ChunkHeader));
	if (strncmp(format.chunk.id, "fmt ", 4) != 0) {
		assert(false);
	}
	// チャンク本体の読み込み
	assert(format.chunk.size <= sizeof(format.fmt));
	file.read((char*)&format.fmt, format.chunk.size);

	// 4. Dataチャンクの読み込み
	ChunkHeader data;
	file.read((char*)&data, sizeof(data));
	// JUNKチャンク等をスキップする処理（必要に応じて）
	while (strncmp(data.id, "data", 4) != 0) {
		file.seekg(data.size, std::ios_base::cur);
		file.read((char*)&data, sizeof(data));
	}

	// 5. 波形データの読み込み
	BYTE* pBuffer = new BYTE[data.size];
	file.read((char*)pBuffer, data.size);

	// 6. Waveファイルを閉じる
	file.close();

	// 7. 戻り値の作成
	SoundData soundData = {};
	soundData.wfex = format.fmt;
	soundData.pBuffer = pBuffer;
	soundData.bufferSize = data.size;

	return soundData;
}

// ====================================================
// オブジェクトデータをまとめる構造体
// ====================================================
struct ObjectData {
	Transform transform;                                // 個別のSTR 
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource; // 個別のWVPバッファ
	TransformationMatrix* wvpData = nullptr;            // マップ用ポインタ
	ModelData modelData;                                // OBJデータ
	ID3D12Resource* vertexResource;                     // 頂点バッファ
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;          // 頂点バッファビュー
};
// オブジェクトの個数
const int kNumObjects = 4;
std::vector<ObjectData> objects(kNumObjects);

// ====================================================
// スプライトデータをまとめる構造体
// ====================================================
struct SpriteData {
	Transform transform;                                // 個別のSTR
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource; // 個別のWVPバッファ
	TransformationMatrix* wvpData = nullptr;            // マップ用ポインタ
};

// スプライトの個数
const int kNumSprites = 1;
std::vector<SpriteData> sprites(kNumSprites);


// =====================
// 音声データ解放関数
// =====================
void SoundUnload(SoundData* soundData) {
	// バッファのメモリを解放
	delete[] soundData->pBuffer;

	soundData->pBuffer = 0;
	soundData->bufferSize = 0;
	soundData->wfex = {};
}

// =====================
// 音声再生関数
// =====================
void SoundPlayWave(IXAudio2* xAudio2, const SoundData& soundData) {
	HRESULT result;

	// 波形フォーマットをもとにSoundVoiceの生成
	IXAudio2SourceVoice* pSourceVoice = nullptr;
	result = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
	assert(SUCCEEDED(result));

	// 再生する波形データの設定
	XAUDIO2_BUFFER buf{};
	buf.pAudioData = soundData.pBuffer;
	buf.AudioBytes = soundData.bufferSize;
	buf.Flags = XAUDIO2_END_OF_STREAM;

	// 波形データの再生
	result = pSourceVoice->SubmitSourceBuffer(&buf);
	result = pSourceVoice->Start();
}

// ===========================
// キーボードの状態を保持する変数 
// ===========================
BYTE key[256] = {};     // 今フレームのキー状態
BYTE keyPre[256] = {};  // 前フレームのキー状態

//  キーを押した状態か（押しっぱなし）
bool IsPressKey(uint8_t keyCode) {
	return (key[keyCode] & 0x80) != 0;
}

//  キーを離した状態か（離しっぱなし）
bool IsReleaseKey(uint8_t keyCode) {
	return (key[keyCode] & 0x80) == 0;
}

// キーを押した瞬間か（トリガー）
bool IsTriggerKey(uint8_t keyCode) {
	return ((key[keyCode] & 0x80) != 0) && ((keyPre[keyCode] & 0x80) == 0);
}

//  キーを離した瞬間か
bool IsReleaseTriggerKey(uint8_t keyCode) {
	return ((key[keyCode] & 0x80) == 0) && ((keyPre[keyCode] & 0x80) != 0);
}


// ====================================================
// ---------- Windowsアプリのエントリーポイント --------- 
// ====================================================
struct D3DResourceLeakChecker {
	~D3DResourceLeakChecker() {
		Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
			debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
		}
	}
};


int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

	D3DResourceLeakChecker leakCheck;

	// ====================================================
	// 初期化
	// ====================================================
	CoInitializeEx(0, COINIT_MULTITHREADED);

	// ===== XAudio2関連の変数宣言 =====
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
	IXAudio2MasteringVoice* masterVoice = nullptr;
	HRESULT result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	// マスターボイスを生成
	result = xAudio2->CreateMasteringVoice(&masterVoice);
	// 音声読み込み 
	SoundData soundData1 = SoundLoadWave("Resources/fanfare.wav");

	// ----- クラッシュハンドラ設定 -----
	SetUnhandledExceptionFilter(ExportDump);


	// ----- ウィンドウクラスの登録とウィンドウの生成 -----  
	WNDCLASS wc{};
	wc.lpfnWndProc = WindowProc;
	wc.lpszClassName = L"CG2WindowClass"; // ウィンドウクラス名
	wc.hInstance = GetModuleHandle(nullptr);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	RegisterClass(&wc);
	const int32_t kClientWidth = 1280; // ゲーム画面の横幅
	const int32_t kClientHeight = 720; // ゲーム画面の縦幅

	RECT wrc = { 0, 0, kClientWidth, kClientHeight };

	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	HWND hwnd = CreateWindow(
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

#ifdef _DEBUG
	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		debugController->EnableDebugLayer();
		debugController->SetEnableGPUBasedValidation(TRUE);
	}

#endif


	// ----- ログシステムの開始 ----- 
	std::ofstream logFile = InitializeLogFile();

	// ----- DXGIファクトリーの生成 ----- 
	// IDXGIFactory7* dxgiFactory = nullptr; // 生ポインタ
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory = nullptr; // ComPtrに変更（自動でReleaseされる）
	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));

	assert(SUCCEEDED(hr));

	// ----- 使用するGPU（アダプタ）の選定 ----- 
	Microsoft::WRL::ComPtr<IDXGIAdapter4> useAdapter = nullptr;

	for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i,
		DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) !=
		DXGI_ERROR_NOT_FOUND; ++i) {


		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = useAdapter->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr));

		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			Log(ConvertString(std::format(L"Use Adapter : {}\n", adapterDesc.Description)), logFile);
			break;
		}
		useAdapter.Reset();
	}


	assert(useAdapter != nullptr);

	// ----- D3D12デバイスの生成 -----
	Microsoft::WRL::ComPtr<ID3D12Device> device;
	D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0 };
	const char* featureLevelStrings[] = { "12.2", "12.1", "12.0" };
	for (size_t i = 0; i < _countof(featureLevels); ++i) {
		hr = D3D12CreateDevice(useAdapter.Get(), featureLevels[i], IID_PPV_ARGS(&device));
		if (SUCCEEDED(hr)) {
			Log(std::format("FeatureLevel : {}\n", featureLevelStrings[i]), logFile);
			break;
		}
	}
	assert(device != nullptr);
	Log("Complete create D3D12Device!!!\n", logFile); // 初期化完了のログをだす


	// --- DriectX初期化処理 ---
	IDirectInput8* directInput = nullptr;
	result = DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8, reinterpret_cast<void**>(&directInput), nullptr);
	assert(SUCCEEDED(result));

	// キーボードデバイスの生成
	IDirectInputDevice8* keyboard = nullptr;
	result = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, nullptr);
	assert(SUCCEEDED(result));

	// 入力データ形式のセット GetDeviceState
	result = keyboard->SetDataFormat(&c_dfDIKeyboard);
	assert(SUCCEEDED(result));

	// 排他制御レベルのセット DirectInput 
	hr = keyboard->SetCooperativeLevel(
		hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY); // DISCL_NONEXCLUSIVE を追加
	assert(SUCCEEDED(hr));

	// マウスデバイスの生成 
	IDirectInputDevice8* mouseDevice = nullptr;
	result = directInput->CreateDevice(GUID_SysMouse, &mouseDevice, nullptr);
	assert(SUCCEEDED(result));

	result = mouseDevice->SetDataFormat(&c_dfDIMouse);
	assert(SUCCEEDED(result));

	result = mouseDevice->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	assert(SUCCEEDED(result));

	// マウスデバイスを作成済みならそれを渡す
	DebugCamera debugCamera;
	debugCamera.Initialize(mouseDevice);

	// デバッグカメラ有効フラグ
	bool useDebugCamera = false;

	// ----- DescriptorSizeを取得しておく -----
	const uint32_t descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	const uint32_t descriptorSizeRTV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	const uint32_t descriptorSizeDSV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);


	// ----- RootSignature の生成 -----　
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// DescriptorTableの設定  
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// RootParameter作成
	D3D12_ROOT_PARAMETER rootParameters[4] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	// マテリアル用 (ピクセルシェーダー / レジスタ b0)
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[1].Descriptor.ShaderRegister = 0;
	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);

	// テクスチャ用 (ピクセルシェーダー / レジスタ t0)
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

	// 平行光源用 (ピクセルシェーダー / レジスタ b1)
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].Descriptor.ShaderRegister = 1;

	// StaticSamplerの設定 
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	// シリアライズしてバイナリにする
	ID3DBlob* signatureBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	hr = D3D12SerializeRootSignature(
		&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&signatureBlob,
		&errorBlob
	);

	if (FAILED(hr)) {
		Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()), logFile);
		assert(false);
	}

	// バイナリをもとに生成 graphicsPipelineStateDesc
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	hr = device->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature)
	);
	assert(SUCCEEDED(hr));

	// ----- dxcCompilerを初期化 -----
	Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils;
	Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler;

	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
	assert(SUCCEEDED(hr));

	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
	assert(SUCCEEDED(hr));


	// ----- include対応の設定 -----
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler;
	hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
	assert(SUCCEEDED(hr));


	// ----- InputLayout の設定 -----
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	// 位置情報
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	// テクスチャ座標
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	// 法線
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// テクスチャ座標
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// ----- RasterizerStateの設定 ----- 
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK; // 背面をカリング
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID; // ポリゴンは塗りつぶす

	// ----- Shaderをコンパイルする ----- 
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob =
		CompileShader(L"Object3D.VS.hlsl",
			L"vs_6_0", dxcUtils, dxcCompiler, includeHandler, logFile);
	assert(vertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob =
		CompileShader(L"Object3D.PS.hlsl",
			L"ps_6_0", dxcUtils, dxcCompiler, includeHandler, logFile);
	assert(pixelShaderBlob != nullptr);


	// ====================================================
	// DepthStencilState の設定
	// ==================================================== 
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true; // 深度テストを有効化
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL; // 深度値を書き込む
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	depthStencilDesc.StencilEnable = false; // ステンシルは今回は使わない
	depthStencilDesc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	depthStencilDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

	// ----- PSOの生成 -----  
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature.Get();
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
	graphicsPipelineStateDesc.VS = {
	vertexShaderBlob->GetBufferPointer(),
	vertexShaderBlob->GetBufferSize()
	};
	graphicsPipelineStateDesc.PS = {
	pixelShaderBlob->GetBufferPointer(),
	pixelShaderBlob->GetBufferSize()
	};
	graphicsPipelineStateDesc.BlendState = blendDesc;
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;

	// DepthStencilの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState;
	hr = device->CreateGraphicsPipelineState(
		&graphicsPipelineStateDesc,
		IID_PPV_ARGS(&graphicsPipelineState)
	);
	assert(SUCCEEDED(hr));


	// =====================================================
	// VertexResourceの生成
	// =====================================================
	// --- モデルデータの読み込み ---
    // ----- VertexResourceの生成 -----
	const uint32_t kSubdivision = 16;
	const uint32_t sphereVertexCount = (kSubdivision + 1) * (kSubdivision + 1); // 球の頂点数
	const uint32_t sphereIndexCount = kSubdivision * kSubdivision * 6; // 球のインデックス数

	// --- 球データ --- 
	Sphere sphere{ {0.0f, 0.0f, 0.0f}, 0.5f };

	// 球用の頂点リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSphere =
		CreateBufferResource(device, sizeof(VertexData) * sphereVertexCount);

	// VertexBufferView設定 
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSphere{};
	vertexBufferViewSphere.BufferLocation = vertexResourceSphere->GetGPUVirtualAddress();
	vertexBufferViewSphere.SizeInBytes = sizeof(VertexData) * sphereVertexCount;
	vertexBufferViewSphere.StrideInBytes = sizeof(VertexData);

	// 頂点データ書き込み　
	VertexData* vertexDataSphere = nullptr;
	vertexResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSphere));
	CreateSphereVertices(sphere, vertexDataSphere, kSubdivision);
	vertexResourceSphere->Unmap(0, nullptr);

	// 3D用マテリアル 

	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource =
		CreateBufferResource(device, sizeof(Material));

	Material* materialData = nullptr;
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	materialData->color = { 1.0f, 1.0f, 1.0f, 1.0f };               // RGBA
	materialData->lightingType = Lighting_HalfLambert;              // 3Dはライティング有効
	materialData->uvTransform = MyMath::Identity();                 // UV変換行列を初期化
	materialResource->Unmap(0, nullptr);


	// 球用のIndex CreateBufferResource
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSphere =
		CreateBufferResource(device, sizeof(uint32_t) * sphereIndexCount);

	D3D12_INDEX_BUFFER_VIEW indexBufferViewSphere{};
	indexBufferViewSphere.BufferLocation = indexResourceSphere->GetGPUVirtualAddress();
	indexBufferViewSphere.SizeInBytes = sizeof(uint32_t) * sphereIndexCount;
	indexBufferViewSphere.Format = DXGI_FORMAT_R32_UINT;

	// インデックスリソースにデータを書き込む 
	uint32_t* indexDataSphere = nullptr;
	indexResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSphere));
	CreateSphereIndices(indexDataSphere, kSubdivision);
	indexResourceSphere->Unmap(0, nullptr);


	// --- OBJモデルデータの読み込み ---
	ModelData multiMeshModel = LoadObjFile("resources", "multiMesh.obj");

	for (auto& mesh : multiMeshModel.meshes) {
		mesh.vertexResource = CreateBufferResource(device, sizeof(VertexData) * mesh.vertices.size());

		mesh.vertexBufferView.BufferLocation = mesh.vertexResource->GetGPUVirtualAddress();
		mesh.vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * mesh.vertices.size());
		mesh.vertexBufferView.StrideInBytes = sizeof(VertexData);

		VertexData* mappedData = nullptr;
		mesh.vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
		std::memcpy(mappedData, mesh.vertices.data(), sizeof(VertexData) * mesh.vertices.size());
		mesh.vertexResource->Unmap(0, nullptr);
	}

	// obj用マテリアル 
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceModel =
		CreateBufferResource(device, sizeof(Material));

	Material* materialDataModel = nullptr;
	materialResourceModel->Map(0, nullptr, reinterpret_cast<void**>(&materialDataModel));
	materialDataModel->color = { 1.0f, 1.0f, 1.0f, 1.0f };               // RGBA
	materialDataModel->lightingType = Lighting_HalfLambert;              // 3Dはライティング有効
	materialDataModel->uvTransform = MyMath::Identity();                 // UV変換行列を初期化
	materialResourceModel->Unmap(0, nullptr);


	// --- Sprite用(2D)の頂点リソースを作る ---
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSprite =
		CreateBufferResource(device, sizeof(VertexData) * 4); // 頂点データが4つ分に変更
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();
	vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 4;// 頂点データが4つ分 
	vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);  // 1頂点あたりのサイズ

	// 矩形追加
	VertexData* vertexDataSprite = nullptr;
	vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));

	// 左下
	vertexDataSprite[0].position = { 0.0f, 360.0f, 0.0f, 1.0f };
	vertexDataSprite[0].texcoord = { 0.0f, 1.0f };
	vertexDataSprite[0].normal = { 0.0f, 0.0f, -1.0f };

	// 左上
	vertexDataSprite[1].position = { 0.0f, 0.0f, 0.0f, 1.0f };
	vertexDataSprite[1].texcoord = { 0.0f, 0.0f };
	vertexDataSprite[1].normal = { 0.0f, 0.0f, -1.0f };

	// 右下
	vertexDataSprite[2].position = { 640.0f, 360.0f, 0.0f, 1.0f };
	vertexDataSprite[2].texcoord = { 1.0f, 1.0f };
	vertexDataSprite[2].normal = { 0.0f, 0.0f, -1.0f };

	// 右上
	vertexDataSprite[3].position = { 640.0f, 0.0f, 0.0f, 1.0f };
	vertexDataSprite[3].texcoord = { 1.0f, 0.0f };
	vertexDataSprite[3].normal = { 0.0f, 0.0f, -1.0f };

	vertexResourceSprite->Unmap(0, nullptr);


	// --- 複数オブジェクト用のデータ作成 ---
	for (size_t i = 0; i < objects.size(); ++i) {
		// それぞれのWVP定数バッファを作成
		objects[i].wvpResource = CreateBufferResource(device, sizeof(TransformationMatrix));
		objects[i].wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&objects[i].wvpData));

		// 初期値設定
		objects[i].wvpData->WVP = MyMath::Identity();
		objects[i].wvpData->World = MyMath::Identity();
	}

	// 初期位置
	objects[0].transform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };  // 球
	objects[1].transform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {5.0f, 0.0f, 0.0f} };  // multiMesh
	objects[2].transform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };  // Teapot
	objects[3].transform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };  // Bunny

	// --- 複数Sprite用のデータ作成 ---
	for (size_t i = 0; i < sprites.size(); ++i) {
		// それぞれのWVP定数バッファを作成
		sprites[i].wvpResource = CreateBufferResource(device, sizeof(TransformationMatrix));
		sprites[i].wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&sprites[i].wvpData));

		// 初期値設定
		sprites[i].wvpData->WVP = MyMath::Identity();
		sprites[i].wvpData->World = MyMath::Identity();
	}

	// 2D Spriteの初期位置
	sprites[0].transform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, { 100.0f, 100.0f, 0.0f} };

	// Sprite用のマテリアルリソースを作成 
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceSprite =
		CreateBufferResource(device, sizeof(Material));
	Material* materialDataSprite = nullptr;
	materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite));
	materialDataSprite->color = { 1.0f, 1.0f, 1.0f, 1.0f };       // RGBA
	materialDataSprite->lightingType = Lighting_None;             // Spriteはライティング無効
	materialDataSprite->uvTransform = MyMath::Identity();         // UV変換行列を初期化 
	materialResourceSprite->Unmap(0, nullptr);

	// Sprite用Index  
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSprite =
		CreateBufferResource(device, sizeof(uint32_t) * 6);

	// View 
	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
	indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();
	indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;

	// インデックスリソースにデータを書き込む
	uint32_t* indexDataSprite = nullptr;
	indexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSprite));
	indexDataSprite[0] = 0; indexDataSprite[1] = 1; indexDataSprite[2] = 2;
	indexDataSprite[3] = 1; indexDataSprite[4] = 3; indexDataSprite[5] = 2;



	// --- 平行光源用 --- 
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource =
		CreateBufferResource(device, sizeof(DirectionalLight));
	DirectionalLight* directionalLightData = nullptr;
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));

	// 初期値 
	directionalLightData->color = { 1.0f,1.0f,1.0f,1.0f };
	directionalLightData->direction = MyMath::Normalize({ 0.0f, -1.0f, 0.0f });
	directionalLightData->intensity = 1.0f;

	// --- サウンド再生 ---
	SoundPlayWave(xAudio2.Get(), soundData1); // 音声データ解放 


	// ----- ViewportとScissorの設定 ----- 
	D3D12_VIEWPORT viewport{};
	viewport.Width = kClientWidth;
	viewport.Height = kClientHeight;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	D3D12_RECT scissorRect{};
	scissorRect.left = 0;
	scissorRect.right = kClientWidth;
	scissorRect.top = 0;
	scissorRect.bottom = kClientHeight;

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

	// ----- コマンドキューの生成 -----  CPUからGPUへの命令の流れを作るためのもの 
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	hr = device->CreateCommandQueue(
		&commandQueueDesc,
		IID_PPV_ARGS(&commandQueue)
	);
	assert(SUCCEEDED(hr));


	// ----- コマンドアロケータの生成 ----- GPUに送る命令を記録するためのもの
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
	hr = device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&commandAllocator)
	);
	assert(SUCCEEDED(hr));

	// ----- コマンドリストの生成 ----- GPUに送る命令を記録するためのもの  
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
	hr = device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		commandAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(&commandList)
	);
	assert(SUCCEEDED(hr));


	// ----- スワップチェーンの生成 ----- 描画していくためのフロントバッファとバックバッファを管理するもの
	IDXGISwapChain4* swapChain = nullptr;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = kClientWidth;
	swapChainDesc.Height = kClientHeight;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue.Get(), hwnd, &swapChainDesc, nullptr, nullptr,
		reinterpret_cast<IDXGISwapChain1**>(&swapChain));
	assert(SUCCEEDED(hr));


	// ----- ディスクリプタヒープの生成 ----- GPUリソースをシェーダーに渡すときのテーブルを管理するもの
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap =
		CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false); // RTV用のヒープ

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap =
		CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true); // SRV用のヒープ


	// ----- テクスチャを呼んで転送する ----- 
	DirectX::ScratchImage mipImages = LoadTexture("Resources/uvChecker.png");
	//DirectX::ScratchImage mipImages = LoadTexture(modelData.material.textureFilePath);
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource =
		CreateTextureResource(device, metadata);
	ID3D12Resource* intermediateResource = UploadTextureData(textureResource.Get(), mipImages, device.Get(), commandList.Get());

	// ２枚目のテクスチャ
	DirectX::ScratchImage mipImages2 = LoadTexture("Resources/monsterBall.png");
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource2 =
		CreateTextureResource(device, metadata2);
	ID3D12Resource* intermediateResource2 = UploadTextureData(textureResource2.Get(), mipImages2, device.Get(), commandList.Get());


	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource =
		CreateDepthStencilTextureResource(device, kClientWidth, kClientHeight);

	// DSV用のディスクリプタヒープを作成
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap =
		CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

	// DSVの設定
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // Resourceと同じフォーマット
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	// DSVHeapの先頭にDSVを作成
	device->CreateDepthStencilView(depthStencilResource.Get(), &dsvDesc, dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	// 1枚目のSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 1);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 1);

	device->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);

	// ２枚目のSRV設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format = metadata2.format;
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

	// index=2 の位置にDescriptorを生成 
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);

	// SRVを生成
	device->CreateShaderResourceView(textureResource2.Get(), &srvDesc2, textureSrvHandleCPU2);

	// 切り替え用 
	bool useMonsterBall = true;

	// ----- SwapChainからResourceを引っ張って来る ----- 描画していくためのフロントバッファとバックバッファを管理するもの
	Microsoft::WRL::ComPtr<ID3D12Resource> swapChainResources[2];
	hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
	assert(SUCCEEDED(hr));
	hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
	assert(SUCCEEDED(hr));

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];

	rtvHandles[0] = rtvStartHandle;
	device->CreateRenderTargetView(swapChainResources[0].Get(), &rtvDesc, rtvHandles[0]);

	rtvHandles[1].ptr = rtvHandles[0].ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	device->CreateRenderTargetView(swapChainResources[1].Get(), &rtvDesc, rtvHandles[1]);


	Microsoft::WRL::ComPtr<ID3D12Fence> fence;
	uint64_t fenceValue = 0;
	hr = device->CreateFence(
		fenceValue,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&fence)
	);
	assert(SUCCEEDED(hr));

	HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent != nullptr);

	hr = commandList->Close();
	assert(SUCCEEDED(hr));

	ID3D12CommandList* commandLists[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(1, commandLists);

	// 実行を待つ
	fenceValue++;
	commandQueue->Signal(fence.Get(), fenceValue);
	if (fence->GetCompletedValue() < fenceValue) {
		fence->SetEventOnCompletion(fenceValue, fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);
	}

	// 実行が完了したので、allocatorとcommandListをResetして次のコマンドを積めるようにする
	hr = commandAllocator->Reset();
	assert(SUCCEEDED(hr));
	hr = commandList->Reset(commandAllocator.Get(), nullptr);
	assert(SUCCEEDED(hr));

	intermediateResource->Release();
	intermediateResource2->Release();




	// ============================================
	// ----- ゲームのメイン処理とメッセージループ -----  
	// ============================================
	// ----- 初期状態のログ出力 -----
	Log("Hello, DirectX!\n", logFile);
	Log(std::format("Window Size : {} x {}\n", kClientWidth, kClientHeight), logFile);

	std::wstring wstr = L"WideStringTest";
	Log(ConvertString(std::format(L"WSTRING:{}\n", wstr)), logFile);

#ifdef USE_IMGUI
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX12_Init(device.Get(),
		swapChainDesc.BufferCount,
		rtvDesc.Format,
		srvDescriptorHeap.Get(),
		srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->Build();
#endif


	// ====================================================
	// メインループ
	// ====================================================
	MSG msg{};
	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			// ----- ゲームの処理 ----- 

			// DirectX毎フレーム処理
			keyboard->Acquire();
			keyboard->GetDeviceState(sizeof(key), key);
			// マウスデバイスの入力取得処理
			mouseDevice->Acquire();

			if (key[DIK_0])
			{
				OutputDebugStringA("Hit 0\n");
			}

#ifdef USE_IMGUI
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();


			// =====================================
			// 3Dモデル
			// =====================================
			ImGui::Begin("BallModel");

			ImGui::SeparatorText("Sphere Model");

			// 色
			static float color[4] = {
				materialData->color.x,
				materialData->color.y,
				materialData->color.z,
				materialData->color.w
			};
			if (ImGui::ColorEdit4("BallModel Color", color)) {
				materialData->color.x = color[0];
				materialData->color.y = color[1];
				materialData->color.z = color[2];
				materialData->color.w = color[3];
			}

			// テクスチャ切り替え
			ImGui::Checkbox("Texture Switching", &useMonsterBall);

			// SRT
			ImGui::DragFloat3("BallModel Scale", &objects[0].transform.scale.x, 0.01f);
			ImGui::SliderAngle("BallModel RotateX", &objects[0].transform.rotate.x);
			ImGui::SliderAngle("BallModel RotateY", &objects[0].transform.rotate.y);
			ImGui::SliderAngle("BallModel RotateZ", &objects[0].transform.rotate.z);
			ImGui::DragFloat3("BallModel Translate", &objects[0].transform.translate.x, 0.1f);

			// ライティング切り替え
			ImGui::Text("Lighting Type");
			ImGui::RadioButton("None", &materialData->lightingType, Lighting_None); ImGui::SameLine();
			ImGui::RadioButton("Lambert", &materialData->lightingType, Lighting_Lambert); ImGui::SameLine();
			ImGui::RadioButton("Half Lambert", &materialData->lightingType, Lighting_HalfLambert);

			

			// =====================================
			// objモデル
			// =====================================
			ImGui::Separator();
			ImGui::SeparatorText("Obj Model");
			
			// 色
			static float colorObj[4] = {
				materialDataModel->color.x,
				materialDataModel->color.y,
				materialDataModel->color.z,
				materialDataModel->color.w
			};
			if (ImGui::ColorEdit4("Obj Color", colorObj)) {
				materialDataModel->color.x = colorObj[0];
				materialDataModel->color.y = colorObj[1];
				materialDataModel->color.z = colorObj[2];
				materialDataModel->color.w = colorObj[3];
			}

			// SRT
			ImGui::DragFloat3("Obj Scale", &objects[1].transform.scale.x, 0.01f);
			ImGui::SliderAngle("Obj RotateX", &objects[1].transform.rotate.x);
			ImGui::SliderAngle("Obj RotateY", &objects[1].transform.rotate.y);
			ImGui::SliderAngle("Obj RotateZ", &objects[1].transform.rotate.z);
			ImGui::DragFloat3("Obj Translate", &objects[1].transform.translate.x, 0.1f);

			// OBJモデルのライティング切替
			ImGui::Text("Obj Lighting Type");
			ImGui::RadioButton("None##Obj", &materialDataModel->lightingType, Lighting_None); ImGui::SameLine();
			ImGui::RadioButton("Lambert##Obj", &materialDataModel->lightingType, Lighting_Lambert); ImGui::SameLine();
			ImGui::RadioButton("Half Lambert##Obj", &materialDataModel->lightingType, Lighting_HalfLambert);
			
			// ---------------------------------
			// ライト共通設定
			// ---------------------------------
			ImGui::SeparatorText("Light");
			// ライトの色
			static float lightColor[4] = {
				directionalLightData->color.x,
				directionalLightData->color.y,
				directionalLightData->color.z,
				directionalLightData->color.w
			};
			if (ImGui::ColorEdit4("Light Color", lightColor)) {
				directionalLightData->color.x = lightColor[0];
				directionalLightData->color.y = lightColor[1];
				directionalLightData->color.z = lightColor[2];
				directionalLightData->color.w = lightColor[3];
			}

			// ライトの向き  
			if (ImGui::SliderFloat3("Light Direction", &directionalLightData->direction.x, -1.0f, 1.0f))
			{
				directionalLightData->direction = MyMath::Normalize(directionalLightData->direction);
			}
			// 光の強さ  
			ImGui::SliderFloat("Intensity", &directionalLightData->intensity, 0.0f, 5.0f);

			// ---------------------------------
			// デバックカメラ切り替え
			// ---------------------------------
			ImGui::SeparatorText("Camera");
			ImGui::Checkbox("Use Debug Camera", &useDebugCamera);

			ImGui::SliderFloat3("CameraTranslate", &cameraTransform.translate.x, -20.0f, 20.0f);
			ImGui::SliderAngle("CameraRotateX", &cameraTransform.rotate.x);
			ImGui::SliderAngle("CameraRotateY", &cameraTransform.rotate.y);
			ImGui::SliderAngle("CameraRotateZ", &cameraTransform.rotate.z);

			ImGui::End();

			

			// =====================================
			// Sprite Material
			// =====================================
			ImGui::Begin("SpriteMaterial");

			// 座標
			ImGui::SeparatorText("Transform");
			ImGui::SliderFloat3("Position", &sprites[0].transform.translate.x, 0.0f, 1280.0f);

			// 色
			static float colorSprite[4] = {
				materialDataSprite->color.x,
				materialDataSprite->color.y,
				materialDataSprite->color.z,
				materialDataSprite->color.w
			};

			if (ImGui::ColorEdit4("Color", colorSprite)) {
				materialDataSprite->color.x = colorSprite[0];
				materialDataSprite->color.y = colorSprite[1];
				materialDataSprite->color.z = colorSprite[2];
				materialDataSprite->color.w = colorSprite[3];
			}


			// スプライトのライティング切替
			ImGui::Text("Sprite Lighting Type");
			ImGui::RadioButton("None##Sprite", &materialDataSprite->lightingType, Lighting_None); ImGui::SameLine();
			ImGui::RadioButton("Lambert##Sprite", &materialDataSprite->lightingType, Lighting_Lambert); ImGui::SameLine();
			ImGui::RadioButton("Half Lambert##Sprite", &materialDataSprite->lightingType, Lighting_HalfLambert);


			// UV変換行列の設定
			ImGui::SeparatorText("UVTransform");
			ImGui::DragFloat2("UVTranslate", &uvTransformSprite.translate.x, 0.01f, -10.0f, 10.0f);
			ImGui::DragFloat2("UVScale", &uvTransformSprite.scale.x, 0.01f, -10.0f, 10.0f);
			ImGui::SliderAngle("UVRotate", &uvTransformSprite.rotate.z);

			ImGui::End();

			// UV変換行列を作る
			Matrix4x4 uvTransformMatrix = MyMath::MakeScaleMatrix(uvTransformSprite.scale);
			uvTransformMatrix = MyMath::Multiply(uvTransformMatrix, MyMath::MakeRotateZMatrix(uvTransformSprite.rotate.z));
			uvTransformMatrix = MyMath::Multiply(uvTransformMatrix, MyMath::MakeTranslateMatrix(uvTransformSprite.translate));

			// マテリアルに反映
			materialDataSprite->uvTransform = uvTransformMatrix;

			ImGui::ShowDemoWindow();

#endif

			UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

			// Transform変数を作る 
			//transform.rotate.y += 0.03f;
			Matrix4x4 worldMatrix = MyMath::MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
			// ビュー行列の計算（フラグによって切り替え）
			// デバッグカメラの更新
			debugCamera.Update();
			Matrix4x4 viewMatrix;
			if (useDebugCamera) {
				// デバッグカメラのビュー行列 
				viewMatrix = debugCamera.GetViewMatrix();
			}
			else {
				// 通常のゲームカメラのビュー行列
				Matrix4x4 cameraMatrix = MyMath::MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
				viewMatrix = MyMath::Inverse(cameraMatrix);
			}
			Matrix4x4 projectionMatrix = MyMath::MakePerspectiveFovMatrix(0.45f, float(kClientWidth) / float(kClientHeight), 0.1f, 100.0f);
			
			// 複数オブジェクトの行列更新
			for (size_t i = 0; i < objects.size(); ++i) {
				Matrix4x4 worldMatrix = MyMath::MakeAffineMatrix(objects[i].transform.scale, objects[i].transform.rotate, objects[i].transform.translate);
				Matrix4x4 worldViewMatrix = MyMath::Multiply(worldMatrix, viewMatrix);
				Matrix4x4 worldViewProjectionMatrix = MyMath::Multiply(worldViewMatrix, projectionMatrix);

				objects[i].wvpData->WVP = worldViewProjectionMatrix;
				objects[i].wvpData->World = worldMatrix;
			}


			// Sprite用行列更新  
			Matrix4x4 viewMatrixSprite = MyMath::Identity();
			Matrix4x4 projectionMatrixSprite = MyMath::MakeOrthographicMatrix(0.0f, 0.0f, float(kClientWidth), float(kClientHeight), 0.0f, 100.0f);

			for (size_t i = 0; i < sprites.size(); ++i) {
				Matrix4x4 worldMatrixSprite = MyMath::MakeAffineMatrix(sprites[i].transform.scale, sprites[i].transform.rotate, sprites[i].transform.translate);
				Matrix4x4 worldViewProjectionMatrixSprite = MyMath::Multiply(worldMatrixSprite, MyMath::Multiply(viewMatrixSprite, projectionMatrixSprite));

				sprites[i].wvpData->WVP = worldViewProjectionMatrixSprite;
				sprites[i].wvpData->World = worldMatrixSprite;
			}


			// TransitionBarrierの設定
			D3D12_RESOURCE_BARRIER barrier{};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition.pResource = swapChainResources[backBufferIndex].Get();
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			commandList->ResourceBarrier(1, &barrier);
			// ----- DSVの設定とクリア -----
			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);
			commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			// 画面クリア 
			float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };
			commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);

			ID3D12DescriptorHeap* heaps[] = { srvDescriptorHeap.Get() };
			commandList->SetDescriptorHeaps(1, heaps);

			// 共通設定 
			commandList->SetGraphicsRootSignature(rootSignature.Get());
			commandList->RSSetViewports(1, &viewport);
			commandList->RSSetScissorRects(1, &scissorRect);
			commandList->SetPipelineState(graphicsPipelineState.Get());

			// =================================  
            // 3D描画
            // =================================  
            // --- 共通設定 ---
			// 平行光源
			commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
			// トポロジ
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


			// 1. 球体の描画 (objects[0])
			// 3D用マテリアル  
			commandList->SetGraphicsRootConstantBufferView(1, materialResource->GetGPUVirtualAddress());
			// 球体のWVP定数バッファ(objects[0])をセット
			commandList->SetGraphicsRootConstantBufferView(0, objects[0].wvpResource->GetGPUVirtualAddress());
			// 球体のテクスチャSRVを設定
			commandList->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);
			// 球体の頂点バッファとインデックスバッファをセット
			commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSphere);
			commandList->IASetIndexBuffer(&indexBufferViewSphere);
			// 描画実行
			commandList->DrawIndexedInstanced(sphereIndexCount, 1, 0, 0, 0);


			// 2. OBJモデルの描画 (objects[1])
			// OBJモデルのWVP定数バッファ(objects[1])をセット
			commandList->SetGraphicsRootConstantBufferView(0, objects[1].wvpResource->GetGPUVirtualAddress());

			// メッシュの数だけループして描画
			for (const auto& mesh : multiMeshModel.meshes) {
				// メッシュごとの頂点バッファをセット
				commandList->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);

				// メッシュごとのマテリアルやテクスチャをセット
				commandList->SetGraphicsRootConstantBufferView(1, materialResourceModel->GetGPUVirtualAddress());
				commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);

				// 描画実行
				commandList->DrawInstanced(UINT(mesh.vertices.size()), 1, 0, 0);
			}

			// =================================
			// Sprite描画　 
			// =================================
			// Sprite用マテリアルをセット
			commandList->SetGraphicsRootConstantBufferView(1, materialResourceSprite->GetGPUVirtualAddress());
			// Spriteのテクスチャ
			commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
			// 頂点バッファ
			commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite);
			// インデックスバッファを設定
			commandList->IASetIndexBuffer(&indexBufferViewSprite);

			// スプライトの数だけWVPを切り替えて描画
			for (size_t i = 0; i < sprites.size(); ++i) {
				// i番目のスプライトのWVPバッファをセット
				commandList->SetGraphicsRootConstantBufferView(0, sprites[i].wvpResource->GetGPUVirtualAddress());
				// 描画実行
				commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
			}



#ifdef USE_IMGUI
			ImGui::Render();
			ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap.Get() };
			commandList->SetDescriptorHeaps(1, descriptorHeaps);
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());
#endif

			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			commandList->ResourceBarrier(1, &barrier);


			hr = commandList->Close();
			assert(SUCCEEDED(hr));

			std::memcpy(keyPre, key, sizeof(key));

			ID3D12CommandList* lists[] = { commandList.Get() };
			commandQueue->ExecuteCommandLists(1, lists);
			swapChain->Present(1, 0);

			fenceValue++;

			commandQueue->Signal(fence.Get(), fenceValue);

			if (fence->GetCompletedValue() < fenceValue)
			{
				fence->SetEventOnCompletion(fenceValue, fenceEvent);
				WaitForSingleObject(fenceEvent, INFINITE);
			}

			// 次のフレーム用の準備
			hr = commandAllocator->Reset();
			assert(SUCCEEDED(hr));

			hr = commandList->Reset(commandAllocator.Get(), nullptr);
			assert(SUCCEEDED(hr));

		}
	}


	// ----- 解放処理 -----
	xAudio2.Reset();              // XAudio2インスタンス解放 
	SoundUnload(&soundData1);     // 音声データ解放
	CloseHandle(fenceEvent);


	// ----- 後処理 リソースチェック -----
	IDXGIDebug1* debug;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
		debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
		debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
		debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
		debug->Release();
	}

	CoUninitialize();


	return 0;
};