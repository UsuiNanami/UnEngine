#define _USE_MATH_DEFINES
#include <cmath>
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
#include "math/MyMath.h"     // 自作の数学ライブラリを使うためのヘッダ
#include "externals/DirectXTex/DirectXTex.h" // テクスチャを読み込むためのライブラリを使うためのヘッダ
#include "externals/DirectXTex/d3dx12.h"     // d3dx12ヘッダー
#include <fstream>      // ifstream 用
#include <sstream>      // istringstream 用（後で行解析に使う）
#include <wrl.h>        // Microsoft::WRL::ComPtrを使うため
#include <xaudio2.h>    // XAudio2を使うため  
#include <fstream>
#define DIRECTINPUT_VERSION 0x0800  // DirectInputのバージョン指定 
#include <dinput.h>
#include "camera/DebugCamera.h" 
#include <vector>
#include <random>
#include <list> // list用

#include "input/Input.h"
#include "base/WinApp.h"
#include "base/DirectXCommon.h"
#include "base/Logger.h"
#include "base/StringUtility.h"
#include "base/D3DResourceLeakChecker.h"

#include "externals/imgui/imgui_impl_win32.h"
#include "externals/imgui/imgui_impl_dx12.h"   // ImGuiのDirectX 12用の実装を使うためのヘッダ

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// *libはヘッダには書いてはいけない！ 
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

// ====================================================
// Transform変数をまとめる構造体 
// ==================================================== 
struct Transform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

// ====================================================
// ParticleForGPU構造体 　
// ====================================================
struct ParticleForGPU {
	Matrix4x4 WVP;
	Matrix4x4 World;
	Vector4 color;
};



Transform transform{ { 1.0f, 1.0f, 1.0f },{ 0.0f, 0.0f, 0.0f },{ 0.0f, 0.0f, 0.0f } };
Transform cameraTransform{ { 1.0f, 1.0f, 1.0f },{ 0.0f, 0.0f, 0.0f },{ 0.0f, 0.0f, -10.0f } }; // カメラのTransform

Transform transformSprite{ { 1.0f, 1.0f, 1.0f },{ 0.0f, 0.0f, 0.0f },{ 0.0f, 0.0f, 0.0f } }; // Sprite用のTransform

// ====================================================
// Particle構造体
// ====================================================
struct Particle {
	Transform transform; // 位置・回転・拡縮
	Vector3 velocity;    // 速度 (m/s)  
	Vector4 color;      // 色 MakeNewParticle関数でランダムに決定する 
	float lifeTime;    // 生存可能な時間 (秒)
	float currentTime; // 発生してからの経過時間 (秒)
	Vector3 startPosition; // 中心の位置
	Vector3 targetOffset;  // 中心から一番遠くまでのずれ
	uint32_t shapeIndex; // どの形か（0〜4） MakeNewParticle
	Vector3 angularVelocity; // 回転の速さ（ラジアン/秒） 
};

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
	float shininess;       // 光沢度
};

// TransformationMatrix構造体の定義
struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
	Matrix4x4 WorldInverseTranspose;
};

// DirectionalLight構造体の定義 
struct DirectionalLight {
	Vector4 color;      // ライトの色 
	Vector3 direction;  // ライトの向き（単位ベクトル）
	float intensity;    // 輝度（明るさ） 
};

// カメラ位置をGPUへ送る構造体
struct CameraForGPU {
	Vector3 worldPosition;
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

// ノード構造体  
struct Node {
	Matrix4x4 localMatrix;       // ノードのLocalMatrix（Transform）
	std::string name;            // ノードの名前
	std::vector<Node> children;  // 子供のノード 
};

// モデル全体の構造体（複数のメッシュをまとめる） 
struct ModelData {
	std::vector<MeshData> meshes;
	Node rootNode;
};

Node ReadNode(aiNode* node) {
	Node result;

	aiMatrix4x4 aiLocalMatrix = node->mTransformation;
	aiLocalMatrix.Transpose(); // assimpは列ベクトル形式なので、転置して行ベクトル形式にする
	for (uint32_t row = 0; row < 4; ++row) {
		for (uint32_t col = 0; col < 4; ++col) {
			result.localMatrix.m[row][col] = aiLocalMatrix[row][col];
		}
	}

	// 名前をコピーする
	result.name = node->mName.C_Str();

	//  子供を読む
	result.children.resize(node->mNumChildren);
	for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
		result.children[childIndex] = ReadNode(node->mChildren[childIndex]);
	}

	return result;
}

// ====================================================
// OBJファイルを読み込む関数
// ====================================================
ModelData LoadModelFile(const std::string& directoryPath, const std::string& filename)
{

	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + filename;
	const aiScene* scene = importer.ReadFile(filePath.c_str(), aiProcess_FlipWindingOrder | aiProcess_FlipUVs);
	assert(scene->HasMeshes()); // メッシュがないのは対応しない

	// 1. 必要な変数を宣言
	ModelData modelData;              // 読み込んだ結果を格納する構造体  

	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
		aiMesh* mesh = scene->mMeshes[meshIndex];
		assert(mesh->HasNormals());        // 法線がないMeshは今回は非対応
		assert(mesh->HasTextureCoords(0)); // TexcoordがないMeshは今回は非対応

		// ここで MeshData を1つ用意する
		MeshData currentMesh;

		// ここにFaceループ
		for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
			aiFace& face = mesh->mFaces[faceIndex];
			assert(face.mNumIndices == 3); // 三角形のみサポート

			for (uint32_t element = 0; element < face.mNumIndices; ++element) {
				uint32_t vertexIndex = face.mIndices[element];
				aiVector3D& position = mesh->mVertices[vertexIndex];
				aiVector3D& normal = mesh->mNormals[vertexIndex];
				aiVector3D& texcoord = mesh->mTextureCoords[0][vertexIndex];

				VertexData vertex;
				vertex.position = { position.x, position.y, position.z, 1.0f };
				vertex.normal = { normal.x, normal.y, normal.z };
				vertex.texcoord = { texcoord.x, texcoord.y };

				// aiProcess_MakeLeftHandedはz*=-1で、右手->左手に変換するので手動で対処 
				vertex.position.x *= -1.0f;
				vertex.normal.x *= -1.0f;

				// 頂点を追加する 
				currentMesh.vertices.push_back(vertex);
			}
		}

		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
		if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0) {
			aiString textureFilePath;
			material->GetTexture(aiTextureType_DIFFUSE, 0, &textureFilePath);
			// テクスチャのパスを保存する 
			currentMesh.material.textureFilePath = directoryPath + "/" + textureFilePath.C_Str();
		}

		// 用意した MeshData を modelData.meshes に追加する    
		modelData.meshes.push_back(currentMesh);

	}
	modelData.rootNode = ReadNode(scene->mRootNode);


	/*
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
		else if (identifier == "mtllib") { // mtlファイルの指定
			std::string materialFilename;
			s >> materialFilename;
			// mtlファイルを解析してテクスチャパス等を取得
			currentMesh.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
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
	*/

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

enum BlendMode {
	// 0: ブレンドなし　
	kBlendModeNone,
	// 1: 通常のαブレンド
	kBlendModeNormal,
	// 2: 加算合成
	kBlendModeAdd,
	// 3: 減算合成
	kBlendModeSubtract,
	// 4: 乗算合成
	kBlendModeMultiply,
	// 5: スクリーン合成
	kBlendModeScreen,
	// 利用してはいけない
	kCountOfBlendModes,
};
// ブレンドモードごとのパイプラインステートを保持する配列
Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineStates[kCountOfBlendModes];


// =========================
// イージング関数
// =========================　

// 出現用：少し行き過ぎて戻る
float EaseOutBack(float t) {
	const float c1 = 1.70158f;
	const float c3 = c1 + 1.0f;
	return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
}

// 消滅用：ゆっくり始まって速く終わる
float EaseInSine(float t) {
	return 1.0f - std::cos(t * static_cast<float>(M_PI) / 2.0f);
}

// 位置用：速く飛び出して、ふわっと止まる　
float EaseOutCubic(float t) {
	return 1.0f - std::pow(1.0f - t, 3.0f);
}

// ===========================
// パーティクル生成関数
// ===========================
Particle MakeNewParticle(std::mt19937& randomEngine, const Vector3& translate) {
	// 位置と速度を [-1, 1] でランダムに設定
	std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
	Particle particle;
	particle.transform.scale = { 0.5f, 0.5f, 0.5f };
	// 最初の向きをランダムに（0〜360°）
	std::uniform_real_distribution<float> distAngle(0.0f, 2.0f * static_cast<float>(M_PI));
	particle.transform.rotate = { distAngle(randomEngine), distAngle(randomEngine), distAngle(randomEngine) };

	// 回転の速さをランダムに（-3〜3 ラジアン/秒 ≒ 1秒で半回転くらい） 
	std::uniform_real_distribution<float> distSpin(-3.0f, 3.0f);
	particle.angularVelocity = { distSpin(randomEngine), distSpin(randomEngine), distSpin(randomEngine) };

	//particle.transform.translate = { distribution(randomEngine), distribution(randomEngine), distribution(randomEngine) };
	particle.velocity = { distribution(randomEngine), distribution(randomEngine), distribution(randomEngine) };

	// 中心（エミッターの位置）から生まれる
	particle.startPosition = translate;
	particle.transform.translate = translate;

	// 飛んでいく方向をランダムに決める　if (isUpdate) {
	Vector3 direction{ distribution(randomEngine), distribution(randomEngine), distribution(randomEngine) };
	if (direction.x == 0.0f && direction.y == 0.0f && direction.z == 0.0f) {
		direction = { 0.0f, 1.0f, 0.0f };
	}
	direction = MyMath::Normalize(direction);

	// どこまで飛ぶか
	std::uniform_real_distribution<float> distDistance(1.5f, 3.0f);
	float distance = distDistance(randomEngine);

	particle.targetOffset = {
		direction.x * distance,
		direction.y * distance,
		direction.z * distance };

	// 色をランダムに設定（0.0f ～ 1.0f）
	std::uniform_real_distribution<float> distColor(0.0f, 1.0f);
	particle.color = { distColor(randomEngine), distColor(randomEngine), distColor(randomEngine), 1.0f };

	// 形をランダムに決める（0〜4）
	std::uniform_int_distribution<uint32_t> distShape(0, 4);
	particle.shapeIndex = distShape(randomEngine);

	// 生存時間をランダムに設定（1秒 ～ 3秒）　 
	std::uniform_real_distribution<float> distTime(1.0f, 3.0f);
	particle.lifeTime = distTime(randomEngine);
	particle.currentTime = 0.0f;

	return particle;
}

// ===========================
// Particleを発生させる装置
// ===========================
struct Emitter {
	Transform transform;   // エミッタのTransform
	uint32_t count;        // 発生数
	float frequency;       // 発生頻度
	float frequencyTime;   // 頻度用時刻
};

// Emitterの設定どおりに、Particleを作ってlistで返す std::list<Particle> particles; 
std::list<Particle> Emit(const Emitter& emitter, std::mt19937& randomEngine) {
	std::list<Particle> particles;
	for (uint32_t count = 0; count < emitter.count; ++count) {
		particles.push_back(MakeNewParticle(randomEngine, emitter.transform.translate));
	}
	return particles;
}

// AABB構造体
struct AABB {
	Vector3 min; // 最小点
	Vector3 max; // 最大点
};

// AccelerationField構造体
struct AccelerationField {
	Vector3 acceleration; // 加速度
	AABB area;            // 範囲 (AABB)
};

// 点がAABBの範囲内に入っているか判定する関数
bool IsCollision(const AABB& aabb, const Vector3& point) {
	return (point.x >= aabb.min.x && point.x <= aabb.max.x) &&
		(point.y >= aabb.min.y && point.y <= aabb.max.y) &&
		(point.z >= aabb.min.z && point.z <= aabb.max.z);
}

// 点光源構造体
struct PointLight {
	Vector4 color;    // 点光源の色 
	Vector3 position; // 点光源の位置
	float intensity;  // 点光源の強度
	float radius;        // 点光源の有効範囲
	float decay;         // 減衰係数
	float padding[2];    // パディング
};

// スポットライト構造体
struct SpotLight {
	Vector4 color;     // スポットライトの色
	Vector3 position;  // スポットライトの位置
	float intensity;   // スポットライトの強度
	Vector3 direction; // スポットライトの方向
	float distance;    // スポットライトの有効距離
	float decay;       // 減衰係数
	float cosAngle;    // スポットライトの角度（ラジアン）
	float cosFalloffStart; // Falloff開始角度（ラジアンのcos)
	float padding[2];  // パディング
};

// ====================================================
// ---------- Windowsアプリのエントリーポイント ---------  
// ====================================================
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

	D3DResourceLeakChecker leakCheck;

	// ====================================================
	// 初期化
	// ====================================================
	// ポインタ置き場　
	Input* input = nullptr;
	WinApp* winApp = nullptr;
	DirectXCommon* dxCommon = nullptr;


	// ===== XAudio2関連の変数宣言 =====
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
	IXAudio2MasteringVoice* masterVoice = nullptr;
	HRESULT result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	// マスターボイスを生成
	result = xAudio2->CreateMasteringVoice(&masterVoice);

	// ----- クラッシュハンドラ設定 ----- WindowProc
	SetUnhandledExceptionFilter(ExportDump);


	// WindowsAPIの初期化
	winApp = new WinApp();
	winApp->Initialize();



	// ----- ログシステムの開始 -----  
	std::ofstream logFile = InitializeLogFile();

	// DirectXの初期化　
	dxCommon = new DirectXCommon();
	dxCommon->Initialize(winApp);

	input = new Input();
	input->Initialize(winApp);

	// デバックカメラの初期化   
	DebugCamera debugCamera;
	debugCamera.Initialize(input->GetMouseDevice());

	// DirectX 12 デバイスとコマンドリストの取得
	ID3D12Device* device = dxCommon->GetDevice();
	ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();

	// デバッグカメラ有効フラグ
	bool useDebugCamera = false;

	// ビルボード有効フラグ
	bool useBillboard = true;

	// Particle更新フラグ useBillboard
	bool isUpdate = true;

	// Particleに掛けるZ軸回転
	float particleRotateZ = 0.0f;

	// フィールド有効化フラグ
	bool useAccelerationField = true;

	// 自動発生するか　
	bool useAutoEmit = false;

	// ----- RootSignature の生成 -----　
	HRESULT hr;
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
	D3D12_ROOT_PARAMETER rootParameters[7] = {};
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

	// カメラ用 (ピクセルシェーダー / レジスタ b2)
	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[4].Descriptor.ShaderRegister = 2;

	// PointLight用 (b3)
	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[5].Descriptor.ShaderRegister = 3;

	// SpotLight用 (b4)
	rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[6].Descriptor.ShaderRegister = 4;

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
		Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
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


	// ----- Particle用 RootSignature の生成 -----   
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignatureParticle{};
	descriptionRootSignatureParticle.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// Instancing用 StructuredBuffer の DescriptorRange (VertexShader / t0)
	D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing[1] = {};
	descriptorRangeForInstancing[0].BaseShaderRegister = 0;
	descriptorRangeForInstancing[0].NumDescriptors = 1;
	descriptorRangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeForInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// RootParameter作成（3つ）Particle
	D3D12_ROOT_PARAMETER rootParametersParticle[3] = {};

	// [0] Instancing用 StructuredBuffer (VertexShader / t0)
	rootParametersParticle[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParametersParticle[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParametersParticle[0].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing;
	rootParametersParticle[0].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing);

	// [1] マテリアル用 (PixelShader / b0)
	rootParametersParticle[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParametersParticle[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParametersParticle[1].Descriptor.ShaderRegister = 0;

	// [2] テクスチャ用 (PixelShader / t0)  ※既存の descriptorRange を使う
	rootParametersParticle[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParametersParticle[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParametersParticle[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParametersParticle[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

	descriptionRootSignatureParticle.pParameters = rootParametersParticle;
	descriptionRootSignatureParticle.NumParameters = _countof(rootParametersParticle);

	// StaticSampler は既存のものを使う
	descriptionRootSignatureParticle.pStaticSamplers = staticSamplers;
	descriptionRootSignatureParticle.NumStaticSamplers = _countof(staticSamplers);

	// シリアライズして生成
	ID3DBlob* signatureBlobParticle = nullptr;
	ID3DBlob* errorBlobParticle = nullptr;
	hr = D3D12SerializeRootSignature(
		&descriptionRootSignatureParticle,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&signatureBlobParticle,
		&errorBlobParticle
	);
	if (FAILED(hr)) {
		Logger::Log(reinterpret_cast<char*>(errorBlobParticle->GetBufferPointer()));
		assert(false);
	}

	Microsoft::WRL::ComPtr<ID3D12RootSignature> particleRootSignature;
	hr = device->CreateRootSignature(
		0,
		signatureBlobParticle->GetBufferPointer(),
		signatureBlobParticle->GetBufferSize(),
		IID_PPV_ARGS(&particleRootSignature)
	);
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
		dxCommon->CompileShader(L"Resources/shaders/Object3D.VS.hlsl",
			L"vs_6_0");
	assert(vertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob =
		dxCommon->CompileShader(L"Resources/shaders/Object3D.PS.hlsl",
			L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

	// ====================================================
	// BlendState の設定 
	// ==================================================== 
	for (uint32_t i = 0; i < kCountOfBlendModes; ++i) {
		D3D12_BLEND_DESC blendDesc{};
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		blendDesc.RenderTarget[0].BlendEnable = true;
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;

		// ====================================================
		// ブレンドモードごとの設定 
		// ====================================================  
		switch (i) {
		case kBlendModeNone: // ブレンドなし
			blendDesc.RenderTarget[0].BlendEnable = false;
			break;

		case kBlendModeNormal: // 通常αブレンド 
			blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
			blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
			break;

		case kBlendModeAdd: // 加算合成
			blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
			blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
			break;

		case kBlendModeSubtract: // 減算合成
			blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
			blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
			blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
			break;

		case kBlendModeMultiply: // 乗算合成
			blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_DEST_COLOR;
			blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
			break;

		case kBlendModeScreen: // スクリーン合成
			blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
			blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
			break;

		}

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

		// ====================================================
		// PSOの生成
		// ====================================================
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

		hr = device->CreateGraphicsPipelineState(
			&graphicsPipelineStateDesc,
			IID_PPV_ARGS(&graphicsPipelineStates[i])
		);
		assert(SUCCEEDED(hr));
	}

	// ----- Particle用 Shaderをコンパイルする -----
	Microsoft::WRL::ComPtr<IDxcBlob> particleVertexShaderBlob =
		dxCommon->CompileShader(L"Resources/shaders/Particle.VS.hlsl",
			L"vs_6_0");
	assert(particleVertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> particlePixelShaderBlob =
		dxCommon->CompileShader(L"Resources/shaders/Particle.PS.hlsl",
			L"ps_6_0");
	assert(particlePixelShaderBlob != nullptr);

	// ----- Particle用 PSO の生成 -----
	// BlendState（通常のαブレンド）
	D3D12_BLEND_DESC particleBlendDesc{};
	particleBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; // RGBAのどこに書き込むか
	particleBlendDesc.RenderTarget[0].BlendEnable = true;                                   // ブレンドの有無
	particleBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;                     // ソースのブレンド係数
	particleBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;                         // ブレンドの演算方法 
	particleBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;                          // デスティネーションのブレンド係数  (確認用)D3D12_BLEND_INV_SRC_ALPHA
	particleBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;                      // αの描く側の係数
	particleBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;                    // αの組み合わせ方
	particleBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;                    // αの背景側の係数



	// DepthStencilState  
	D3D12_DEPTH_STENCIL_DESC particleDepthStencilDesc{};
	particleDepthStencilDesc.DepthEnable = true; // 深度テストを有効化 
	particleDepthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // 深度値を書きこまない
	particleDepthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC particlePsoDesc{};
	particlePsoDesc.pRootSignature = particleRootSignature.Get();   // Particle用 
	particlePsoDesc.InputLayout = inputLayoutDesc;
	particlePsoDesc.VS = { particleVertexShaderBlob->GetBufferPointer(), particleVertexShaderBlob->GetBufferSize() };
	particlePsoDesc.PS = { particlePixelShaderBlob->GetBufferPointer(), particlePixelShaderBlob->GetBufferSize() };
	particlePsoDesc.BlendState = particleBlendDesc;
	particlePsoDesc.RasterizerState = rasterizerDesc;
	particlePsoDesc.DepthStencilState = particleDepthStencilDesc;
	particlePsoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	particlePsoDesc.NumRenderTargets = 1;
	particlePsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	particlePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	particlePsoDesc.SampleDesc.Count = 1;
	particlePsoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> particlePipelineState;
	hr = device->CreateGraphicsPipelineState(&particlePsoDesc, IID_PPV_ARGS(&particlePipelineState));
	assert(SUCCEEDED(hr));

	// ===================================================== 
	// VertexResourceの生成 
	// =====================================================
	// 四角形(Primitive)をコード上で構築する
	/*ModelData fenceModel;
	{
		MeshData quad;
		// 左上, 右上, 左下 / 左下, 右上, 右下（時計回り＝表）
		quad.vertices.push_back({ { -1.0f,  1.0f, 0.0f, 1.0f },{ 0.0f, 0.0f },{ 0.0f, 0.0f, -1.0f } });
		quad.vertices.push_back({ { 1.0f,  1.0f, 0.0f, 1.0f },{ 1.0f, 0.0f },{ 0.0f, 0.0f, -1.0f } });
		quad.vertices.push_back({ { -1.0f, -1.0f, 0.0f, 1.0f },{ 0.0f, 1.0f },{ 0.0f, 0.0f, -1.0f } });
		quad.vertices.push_back({ { -1.0f, -1.0f, 0.0f, 1.0f },{ 0.0f, 1.0f },{ 0.0f, 0.0f, -1.0f } });
		quad.vertices.push_back({ { 1.0f,  1.0f, 0.0f, 1.0f },{ 1.0f, 0.0f },{ 0.0f, 0.0f, -1.0f } });
		quad.vertices.push_back({ { 1.0f, -1.0f, 0.0f, 1.0f },{ 1.0f, 1.0f },{ 0.0f, 0.0f, -1.0f } });
		quad.material.textureFilePath = "Resources/circle.png";
		fenceModel.meshes.push_back(quad);
	}
	*/

	// パーティクル（5種類） 
	const uint32_t kNumShapes = 5;
	ModelData particleModels[kNumShapes];
	for (uint32_t i = 0; i < kNumShapes; ++i) {
		std::string name = "Particle" + std::to_string(i + 1);   // "Particle1"〜"Particle5"
		particleModels[i] = LoadModelFile("Resources/" + name, name + ".obj");

		// テクスチャは全部同じ
		for (auto& mesh : particleModels[i].meshes) {
			mesh.material.textureFilePath = "Resources/Particle.png";
		}
	}

	// ====================================================
	// 球(Sphere)の頂点バッファ・インデックスバッファ
	// ====================================================
	const uint32_t kSubdivision = 16; // 球の分割数
	const uint32_t kSphereVertexCount = (kSubdivision + 1) * (kSubdivision + 1);
	const uint32_t kSphereIndexCount = kSubdivision * kSubdivision * 6;

	// --- 頂点バッファ ---
	Microsoft::WRL::ComPtr<ID3D12Resource> sphereVertexResource =
		dxCommon->CreateBufferResource(sizeof(VertexData) * kSphereVertexCount);

	D3D12_VERTEX_BUFFER_VIEW sphereVertexBufferView{};
	sphereVertexBufferView.BufferLocation = sphereVertexResource->GetGPUVirtualAddress();
	sphereVertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * kSphereVertexCount);
	sphereVertexBufferView.StrideInBytes = sizeof(VertexData);

	VertexData* sphereVertexData = nullptr;
	sphereVertexResource->Map(0, nullptr, reinterpret_cast<void**>(&sphereVertexData));
	Sphere sphere{ { 0.0f, 0.0f, 0.0f }, 1.0f }; // 中心と半径 
	CreateSphereVertices(sphere, sphereVertexData, kSubdivision);
	sphereVertexResource->Unmap(0, nullptr);

	// ====================================================
	// フィールド
	// ====================================================
	// ModelData terrainModel = LoadModelFile("resources/terrain", "terrain.obj");
	ModelData terrainModel = LoadModelFile("resources", "plane.gltf");

	for (auto& mesh : terrainModel.meshes) {
		mesh.vertexResource = dxCommon->CreateBufferResource(sizeof(VertexData) * mesh.vertices.size());

		mesh.vertexBufferView.BufferLocation = mesh.vertexResource->GetGPUVirtualAddress();
		mesh.vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * mesh.vertices.size());
		mesh.vertexBufferView.StrideInBytes = sizeof(VertexData);

		VertexData* mappedData = nullptr;
		mesh.vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
		std::memcpy(mappedData, mesh.vertices.data(), sizeof(VertexData) * mesh.vertices.size());
		mesh.vertexResource->Unmap(0, nullptr);
	}

	// --- インデックスバッファ ---
	Microsoft::WRL::ComPtr<ID3D12Resource> sphereIndexResource =
		dxCommon->CreateBufferResource(sizeof(uint32_t) * kSphereIndexCount);

	D3D12_INDEX_BUFFER_VIEW sphereIndexBufferView{};
	sphereIndexBufferView.BufferLocation = sphereIndexResource->GetGPUVirtualAddress();
	sphereIndexBufferView.SizeInBytes = UINT(sizeof(uint32_t) * kSphereIndexCount);
	sphereIndexBufferView.Format = DXGI_FORMAT_R32_UINT;

	// インデックスバッファにデータ転送 
	uint32_t* sphereIndexData = nullptr;
	sphereIndexResource->Map(0, nullptr, reinterpret_cast<void**>(&sphereIndexData));
	CreateSphereIndices(sphereIndexData, kSubdivision);
	sphereIndexResource->Unmap(0, nullptr);

	// パーティクルの形（5種類）の頂点バッファを作る
	for (auto& model : particleModels) {
		for (auto& mesh : model.meshes) {
			mesh.vertexResource = dxCommon->CreateBufferResource(sizeof(VertexData) * mesh.vertices.size());

			mesh.vertexBufferView.BufferLocation = mesh.vertexResource->GetGPUVirtualAddress();
			mesh.vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * mesh.vertices.size());
			mesh.vertexBufferView.StrideInBytes = sizeof(VertexData);

			VertexData* mappedData = nullptr;
			mesh.vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
			std::memcpy(mappedData, mesh.vertices.data(), sizeof(VertexData) * mesh.vertices.size());
			mesh.vertexResource->Unmap(0, nullptr);
		}
	}

	// Obj用マテリアル
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceFence =
		dxCommon->CreateBufferResource(sizeof(Material));

	Material* materialDataFence = nullptr;
	materialResourceFence->Map(0, nullptr, reinterpret_cast<void**>(&materialDataFence));
	materialDataFence->color = { 1.0f, 1.0f, 1.0f, 1.0f };               // RGBA
	materialDataFence->lightingType = Lighting_HalfLambert;              // 3Dはライティング有効
	materialDataFence->uvTransform = MyMath::Identity();                 // UV変換行列を初期化
	materialDataFence->shininess = 50.0f;                                // 光沢度
	materialResourceFence->Unmap(0, nullptr);

	// パーティクル用マテリアル
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceParticle =
		dxCommon->CreateBufferResource(sizeof(Material));

	Material* materialDataParticle = nullptr;
	materialResourceParticle->Map(0, nullptr, reinterpret_cast<void**>(&materialDataParticle));
	materialDataParticle->color = { 1.0f, 1.0f, 1.0f, 1.0f };   // 白（＝色を変えない）
	materialDataParticle->lightingType = Lighting_None;         // パーティクルはライティングしない
	materialDataParticle->uvTransform = MyMath::Identity();


	// フェンス用のTransform変数とWVPリソース
	Transform fenceTransform = { { 1.0f, 1.0f, 1.0f },{ 0.0f, 0.0f, 0.0f },{ 0.0f, 0.0f, 0.0f } };

	Microsoft::WRL::ComPtr<ID3D12Resource> fenceWvpResource = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
	TransformationMatrix* fenceWvpData = nullptr;
	fenceWvpResource->Map(0, nullptr, reinterpret_cast<void**>(&fenceWvpData));
	fenceWvpData->WVP = MyMath::Identity();
	fenceWvpData->World = MyMath::Identity();
	fenceWvpData->WorldInverseTranspose = MyMath::Identity();

	// フィールド用マテリアル
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceTerrain =
		dxCommon->CreateBufferResource(sizeof(Material));

	Material* materialDataTerrain = nullptr;
	materialResourceTerrain->Map(0, nullptr, reinterpret_cast<void**>(&materialDataTerrain));
	materialDataTerrain->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialDataTerrain->lightingType = Lighting_Lambert;   // 下の補足を参照
	materialDataTerrain->uvTransform = MyMath::Identity();
	materialDataTerrain->shininess = 50.0f;
	materialResourceTerrain->Unmap(0, nullptr);

	// フィールド用のTransform変数とWVPリソース
	Transform terrainTransform = { { 1.0f, 1.0f, 1.0f },{ 0.0f, 0.0f, 0.0f },{ 0.0f, 0.0f, 0.0f } };

	Microsoft::WRL::ComPtr<ID3D12Resource> terrainWvpResource =
		dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
	TransformationMatrix* terrainWvpData = nullptr;
	terrainWvpResource->Map(0, nullptr, reinterpret_cast<void**>(&terrainWvpData));
	terrainWvpData->WVP = MyMath::Identity();
	terrainWvpData->World = MyMath::Identity();
	terrainWvpData->WorldInverseTranspose = MyMath::Identity();


	// --- 平行光源用 ---  　 
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource =
		dxCommon->CreateBufferResource(sizeof(DirectionalLight));
	DirectionalLight* directionalLightData = nullptr;
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));

	// --- 点光源用 ---
	Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource =
		dxCommon->CreateBufferResource(sizeof(PointLight));
	PointLight* pointLightData = nullptr;
	pointLightResource->Map(0, nullptr, reinterpret_cast<void**>(&pointLightData));

	// --- スポットライト用 ---
	Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource =
		dxCommon->CreateBufferResource(sizeof(SpotLight));
	SpotLight* spotLightData = nullptr;
	spotLightResource->Map(0, nullptr, reinterpret_cast<void**>(&spotLightData));

	// スポットライトの初期値設定
	spotLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	spotLightData->position = { 2.0f, 1.25f, 0.0f };
	spotLightData->distance = 7.0f;
	spotLightData->direction = MyMath::Normalize({ -1.0f, -1.0f, 0.0f });
	spotLightData->intensity = 4.0f;
	spotLightData->decay = 2.0f;
	spotLightData->cosAngle = std::cos(static_cast<float>(M_PI) / 3.0f);
	spotLightData->cosFalloffStart = std::cos(static_cast<float>(M_PI) / 6.0f);
	// 角度の初期値
	static float spotAngle = static_cast<float>(M_PI) / 3.0f;
	static float falloffStartAngle = static_cast<float>(M_PI) / 6.0f;
	spotLightData->cosAngle = std::cos(spotAngle);
	spotLightData->cosFalloffStart = std::cos(falloffStartAngle);

	// 平行光源の初期値設定 
	directionalLightData->color = { 1.0f,1.0f,1.0f,1.0f };
	directionalLightData->direction = MyMath::Normalize({ 0.0f, -1.0f, 0.0f });
	directionalLightData->intensity = 1.0f;

	// 点光源の初期値設定 
	pointLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	pointLightData->position = { 0.0f, 2.0f, 0.0f };
	pointLightData->intensity = 1.0f;
	pointLightData->radius = 10.0f;
	pointLightData->decay = 2.0f;


	// --- カメラ用 ---
	Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource =
		dxCommon->CreateBufferResource(sizeof(CameraForGPU));
	CameraForGPU* cameraData = nullptr;
	cameraResource->Map(0, nullptr, reinterpret_cast<void**>(&cameraData));
	cameraData->worldPosition = cameraTransform.translate; // 初期値 

	// --- サウンド再生 ---
	// SoundPlayWave(xAudio2.Get(), soundData1); // 音声データ解放

	// ----- DescriptorSizeを取得しておく ----- 
	const uint32_t descriptorSizeSRV = dxCommon->GetDescriptorSizeSRV();
	const uint32_t descriptorSizeRTV = dxCommon->GetDescriptorSizeRTV();
	const uint32_t descriptorSizeDSV = dxCommon->GetDescriptorSizeDSV();

	// ----- テクスチャを呼んで転送する -----  
	// 1枚目
	DirectX::ScratchImage mipImages = DirectXCommon::LoadTexture("Resources/Particle.png");
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource =
		dxCommon->CreateTextureResource(metadata);
	ID3D12Resource* intermediateResource = dxCommon->UploadTextureData(textureResource.Get(), mipImages);

	// ２枚目
	DirectX::ScratchImage mipImages2 = DirectXCommon::LoadTexture("Resources/monsterBall.png");
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource2 =
		dxCommon->CreateTextureResource(metadata2);
	ID3D12Resource* intermediateResource2 = dxCommon->UploadTextureData(textureResource2.Get(), mipImages2);

	// ----- 1枚目のSRVの設定 -----
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = dxCommon->GetSRVCPUDescriptorHandle(1);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = dxCommon->GetSRVGPUDescriptorHandle(1);

	dxCommon->GetDevice()->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);

	// ====================================================
	// Instancing用 Resource / SRV 
	// ====================================================
	const uint32_t kNumMaxInstances = 100; // 最大インスタンス数 

	// TransformationMatrix を kNumInstances 個分格納できる 
	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource =
		dxCommon->CreateBufferResource(sizeof(ParticleForGPU) * kNumMaxInstances * kNumShapes);

	ParticleForGPU* instancingData = nullptr;
	instancingResource->Map(0, nullptr, reinterpret_cast<void**>(&instancingData));
	for (uint32_t index = 0; index < kNumMaxInstances * kNumShapes; ++index) {
		instancingData[index].WVP = MyMath::Identity();
		instancingData[index].World = MyMath::Identity();
		instancingData[index].color = { 1.0f, 1.0f, 1.0f, 1.0f };
	}

	// StructuredBuffer 用の SRV (SRVヒープの index 3)
	D3D12_SHADER_RESOURCE_VIEW_DESC instancingSrvDesc{};
	instancingSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	instancingSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	instancingSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	instancingSrvDesc.Buffer.FirstElement = 0;
	instancingSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	instancingSrvDesc.Buffer.NumElements = kNumMaxInstances;
	instancingSrvDesc.Buffer.StructureByteStride = sizeof(ParticleForGPU);

	// 形ごとの SRV（SRVヒープの index 5〜9）
	D3D12_GPU_DESCRIPTOR_HANDLE shapeSrvHandleGPU[kNumShapes];
	for (uint32_t i = 0; i < kNumShapes; ++i) {
		D3D12_SHADER_RESOURCE_VIEW_DESC desc = instancingSrvDesc; 
		desc.Buffer.FirstElement = i * kNumMaxInstances;        
		desc.Buffer.NumElements = kNumMaxInstances;

		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = dxCommon->GetSRVCPUDescriptorHandle(5 + i);
		shapeSrvHandleGPU[i] = dxCommon->GetSRVGPUDescriptorHandle(5 + i);
		device->CreateShaderResourceView(instancingResource.Get(), &desc, cpuHandle);
	}


	D3D12_CPU_DESCRIPTOR_HANDLE instancingSrvHandleCPU = dxCommon->GetSRVCPUDescriptorHandle(3);
	D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU = dxCommon->GetSRVGPUDescriptorHandle(3);
	device->CreateShaderResourceView(instancingResource.Get(), &instancingSrvDesc, instancingSrvHandleCPU);

	// ====================================================
	// 乱数生成器の初期化
	// ====================================================  
	std::random_device seedGenerator;
	std::mt19937 randomEngine(seedGenerator());

	// 一様分布生成器（-1.0f ～ 1.0f）
	std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);

	// ----- フレームレート固定用のデルタタイム -----
	const float kDeltaTime = 1.0f / 60.0f; // 60fps固定  

	// ====================================================
	// Particleの初期化
	// ====================================================　 
	//Particle particles[kNumMaxInstances]; 
	//for (uint32_t index = 0; index < kNumMaxInstances; ++index) {
	//	particles[index] = MakeNewParticle(randomEngine);　Vector3 randomTranslate
	//}

	Emitter emitter{};             // Emitter構造体の初期化
	emitter.count = 3;             // 1フレームで生成するパーティクルの数 
	emitter.frequency = 0.5f;      // 0.5秒ごとに発生 
	emitter.frequencyTime = 0.0f;  // 頻度用の時刻。0で初期化  
	emitter.transform.translate = { 0.0f, 0.0f, 0.0f };
	emitter.transform.rotate = { 0.0f, 0.0f, 0.0f };
	emitter.transform.scale = { 1.0f, 1.0f, 1.0f };

	// 加速度フィールドの初期化
	AccelerationField accelerationField;
	accelerationField.acceleration = { 15.0f, 0.0f, 0.0f }; // X方向に加速する
	accelerationField.area.min = { -1.0f, -1.0f, -1.0f };
	accelerationField.area.max = { 1.0f,  1.0f,  1.0f };

	// listで管理する場合
	std::list<Particle> particles;
	//for (uint32_t index = 0; index < kNumMaxInstances; ++index) { 
	//	particles.push_back(MakeNewParticle(randomEngine, emitter.transform.translate));
	//}

	// ２枚目のSRV設定  　
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format = metadata2.format;
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

	// index=2 の位置にDescriptorを生成 MakeNewParticle
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = dxCommon->GetSRVCPUDescriptorHandle(2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = dxCommon->GetSRVGPUDescriptorHandle(2);

	// SRVを生成 
	device->CreateShaderResourceView(textureResource2.Get(), &srvDesc2, textureSrvHandleCPU2);

	// ----- フィールド用テクスチャ(3枚目)-----
	DirectX::ScratchImage mipImages3 = DirectXCommon::LoadTexture(terrainModel.meshes[0].material.textureFilePath);
	const DirectX::TexMetadata& metadata3 = mipImages3.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource3 =
		dxCommon->CreateTextureResource(metadata3);
	ID3D12Resource* intermediateResource3 = dxCommon->UploadTextureData(textureResource3.Get(), mipImages3);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc3{};
	srvDesc3.Format = metadata3.format;
	srvDesc3.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc3.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc3.Texture2D.MipLevels = UINT(metadata3.mipLevels);

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU3 = dxCommon->GetSRVCPUDescriptorHandle(4);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU3 = dxCommon->GetSRVGPUDescriptorHandle(4);
	device->CreateShaderResourceView(textureResource3.Get(), &srvDesc3, textureSrvHandleCPU3);

	// 切り替え用 　
	bool useMonsterBall = true;

	// コマンドリストをクローズ 
	hr = commandList->Close();
	assert(SUCCEEDED(hr));

	// GPUに初期化コマンドを実行させる
	ID3D12CommandList* commandLists[] = { commandList };
	dxCommon->GetCommandQueue()->ExecuteCommandLists(1, commandLists);

	// 初期化用のテクスチャ転送等の完了を待つ
	static uint64_t initFenceValue = 0;
	initFenceValue++;
	dxCommon->GetCommandQueue()->Signal(dxCommon->GetFence(), initFenceValue);
	if (dxCommon->GetFence()->GetCompletedValue() < initFenceValue) {
		dxCommon->GetFence()->SetEventOnCompletion(initFenceValue, dxCommon->GetFenceEvent());
		WaitForSingleObject(dxCommon->GetFenceEvent(), INFINITE);
	}


	// メインループで使えるようにコマンドリストとアロケータをリセットする
	hr = dxCommon->GetCommandAllocator()->Reset();
	assert(SUCCEEDED(hr));
	hr = commandList->Reset(dxCommon->GetCommandAllocator(), nullptr);
	assert(SUCCEEDED(hr));

	// 一時転送バッファの解放
	intermediateResource->Release();
	intermediateResource2->Release();
	intermediateResource3->Release();


	// ============================================
	// ----- ゲームのメイン処理とメッセージループ -----
	// ============================================ 
	// ----- 初期状態のログ出力 -----
	Logger::Log("Hello, DirectX!\n");
	Logger::Log(std::format("Window Size : {} x {}\n", WinApp::kClientWidth, WinApp::kClientHeight));

	std::wstring wstr = L"WideStringTest";
	Logger::Log(StringUtility::ConvertString(std::format(L"WSTRING:{}\n", wstr)));


	// ====================================================
	// メインループ
	// ==================================================== 
	while (true) {
		// Windowsのメッセージ処理
		if (winApp->ProcessMessage()) {
			// ゲームループを抜ける
			break;
		}
		// ----- ゲームの処理 -----

		// 入力の更新
		input->Update();

		// ---------- マウス ----------
		// 左クリックの「押した瞬間」を判定する
		static bool preMouseLeft = false;
		bool mouseLeft = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0; // 今押されているか
		bool mouseTrigger = mouseLeft && !preMouseLeft;                 // 今押されていて、前は押されていない
		preMouseLeft = mouseLeft;

		// マウスの位置（ウィンドウの左上が(0,0)のピクセル座標）
		POINT mousePos;
		GetCursorPos(&mousePos);                       // 画面全体での位置
		ScreenToClient(winApp->GetHwnd(), &mousePos);  // ウィンドウの中での位置に変換

		// PushKey から TriggerKey に変更
		if (input->TriggerKey(DIK_0))
		{
			OutputDebugStringA("Hit 0\n");
		}

#ifdef USE_IMGUI
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("入力テスト");

		// PushKey（押しっぱなし判定）の確認
		if (input->PushKey(DIK_0)) {
			ImGui::Text("0キー : ON");
		}
		else {
			ImGui::Text("0キー : OFF");
		}

		// 2. TriggerKey（押した瞬間判定）の確認（回数をカウント）
		static int hitCount = 0;
		if (input->TriggerKey(DIK_0)) {
			hitCount++; // 押された瞬間だけ +1 される
		}
		ImGui::Text("0キー 押された回数: %d", hitCount);

		ImGui::End();

		// ---------------------------------
		// ライト共通設定
		// ---------------------------------
		// --- 平行光源設定 ---
		ImGui::SeparatorText("平行光源設定");
		// ライトの色
		static float lightColor[4] = {
			directionalLightData->color.x,
			directionalLightData->color.y,
			directionalLightData->color.z,
			directionalLightData->color.w
		};
		if (ImGui::ColorEdit4("ライトの色##平行", lightColor)) {
			directionalLightData->color.x = lightColor[0];
			directionalLightData->color.y = lightColor[1];
			directionalLightData->color.z = lightColor[2];
			directionalLightData->color.w = lightColor[3];
		}

		// ライトの方向 
		if (ImGui::SliderFloat3("ライトの方向##平行", &directionalLightData->direction.x, -1.0f, 1.0f)) {
			Vector3& d = directionalLightData->direction;
			float lengthSq = d.x * d.x + d.y * d.y + d.z * d.z;
			if (lengthSq > 0.0001f) {
				d = MyMath::Normalize(d);
			}
			else {
				d = { 0.0f, -1.0f, 0.0f }; // 0のときは初期の向きに戻す
			}
		}
		ImGui::SliderFloat("ライトの強度##平行", &directionalLightData->intensity, 0.0f, 5.0f);

		// --- 点光源 ---
		ImGui::SeparatorText("点光源設定");
		ImGui::ColorEdit4("ライトの色##点", &pointLightData->color.x);
		ImGui::DragFloat3("ライトの位置##点", &pointLightData->position.x, 0.1f);
		ImGui::SliderFloat("ライトの強度##点", &pointLightData->intensity, 0.0f, 5.0f);

		// --- スポットライト ---　
		ImGui::SeparatorText("スポットライト");
		ImGui::ColorEdit4("ライトの色##スポット", &spotLightData->color.x);
		ImGui::DragFloat3("ライトの位置##スポット", &spotLightData->position.x, 0.1f);
		ImGui::SliderFloat("ライトの強度##スポット", &spotLightData->intensity, 0.0f, 5.0f);

		// 角度調整用スライダー
		bool angleChanged = false;
		angleChanged |= ImGui::SliderAngle("照射角度", &spotAngle, 0.0f, 89.0f);
		angleChanged |= ImGui::SliderAngle("減衰開始角度", &falloffStartAngle, 0.0f, 89.0f);
		// 角度の制限
		if (falloffStartAngle >= spotAngle) {
			falloffStartAngle = spotAngle - 0.001f;
		}
		// cos値を計算してGPU用データに書き込む
		if (angleChanged) {
			spotLightData->cosAngle = std::cos(spotAngle);
			spotLightData->cosFalloffStart = std::cos(falloffStartAngle);
		}

		// ---------------------------------
		// デバックカメラ切り替え 
		// ---------------------------------　　particle.velocity
		ImGui::SeparatorText("カメラ");
		ImGui::Checkbox("デバッグカメラを使う", &useDebugCamera);

		ImGui::SliderFloat3("位置##カメラ", &cameraTransform.translate.x, -20.0f, 20.0f);
		ImGui::SliderAngle("回転X##カメラ", &cameraTransform.rotate.x);
		ImGui::SliderAngle("回転Y##カメラ", &cameraTransform.rotate.y);
		ImGui::SliderAngle("回転Z##カメラ", &cameraTransform.rotate.z);


		// =====================================
		// パーティクルの設定
		// ===================================== 　
		ImGui::Begin("パーティクル設定");
		//ImGui::SeparatorText("球	");
		//ImGui::ColorEdit4("色##球", &materialDataFence->color.x);
		//ImGui::DragFloat3("位置##球", &fenceTransform.translate.x, 0.1f);
		//ImGui::SliderAngle("回転X##球", &fenceTransform.rotate.x);
		//ImGui::SliderAngle("回転Y##球", &fenceTransform.rotate.y);
		//ImGui::SliderAngle("回転Z##球", &fenceTransform.rotate.z);
		//ImGui::DragFloat3("サイズ", &fenceTransform.scale.x, 0.1f, 0.01f, 10.0f);

		ImGui::SeparatorText("パーティクル");
		// マテリアルカラー調整（任意）　 
		ImGui::Text("操作説明");
		ImGui::Text("左クリック：クリックした場所にパーティクルを発生");
		ImGui::ColorEdit4("色##パーティクル", &materialDataParticle->color.x);
		// listで管理する場合
		if (ImGui::Button("リスタート")) {
			particles.clear();
			for (uint32_t index = 0; index < kNumMaxInstances; ++index) {
				particles.push_back(MakeNewParticle(randomEngine, emitter.transform.translate));
			}
		}
		// Particleを追加する 　
		if (ImGui::Button("パーティクル追加")) {
			particles.splice(particles.end(), Emit(emitter, randomEngine));
		}

		// Billboard切り替え
		ImGui::Checkbox("ビルボード", &useBillboard);
		ImGui::Checkbox("自動発生", &useAutoEmit);
		ImGui::DragFloat3("エミッターの位置", &emitter.transform.translate.x, 0.01f, -100.0f, 100.0f);
		ImGui::Checkbox("再生", &isUpdate);

		// Particleの回転角度を調整する
		ImGui::SliderAngle("Z回転", &particleRotateZ);

		//　フィールドON/OFFを切り替える
		//ImGui::Checkbox("加速度フィールド", &useAccelerationField);
		//ImGui::DragFloat3("加速度##フィールド", &accelerationField.acceleration.x, 0.1f);
		//ImGui::DragFloat3("範囲の最小##フィールド", &accelerationField.area.min.x, 0.1f);
		//ImGui::DragFloat3("範囲の最大##フィールド", &accelerationField.area.max.x, 0.1f);

		ImGui::Text("マウス: %d, %d", mousePos.x, mousePos.y);
		static int clickCount = 0;
		if (mouseTrigger) { clickCount++; }
		ImGui::Text("クリック回数: %d", clickCount);

		ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
		ImGui::End();

		ImGui::ShowDemoWindow();

#endif

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
		Matrix4x4 projectionMatrix = MyMath::MakePerspectiveFovMatrix(0.45f, float(WinApp::kClientWidth) / float(WinApp::kClientHeight), 0.1f, 100.0f);

		// ---------- クリックした場所からパーティクルを出す ----------
		bool clickOnUI = false;
#ifdef USE_IMGUI
		clickOnUI = ImGui::GetIO().WantCaptureMouse; // ImGuiのウィンドウ上のクリックは無視
#endif
		if (mouseTrigger && !clickOnUI) {
			float ndcX = (float(mousePos.x) / float(WinApp::kClientWidth)) * 2.0f - 1.0f;
			float ndcY = 1.0f - (float(mousePos.y) / float(WinApp::kClientHeight)) * 2.0f;

			Matrix4x4 inverseViewProjection = MyMath::Inverse(MyMath::Multiply(viewMatrix, projectionMatrix));
			Vector3 nearPoint = MyMath::Transform({ ndcX, ndcY, 0.0f }, inverseViewProjection); // 手前
			Vector3 farPoint = MyMath::Transform({ ndcX, ndcY, 1.0f }, inverseViewProjection); // 奥 

			float rate = -nearPoint.z / (farPoint.z - nearPoint.z);
			Vector3 clickPosition = {
				nearPoint.x + (farPoint.x - nearPoint.x) * rate,
				nearPoint.y + (farPoint.y - nearPoint.y) * rate,
				0.0f };

			const uint32_t kBurstCount = 10;
			for (uint32_t i = 0; i < kBurstCount; ++i) {
				particles.push_back(MakeNewParticle(randomEngine, clickPosition));
			}
		}

		// カメラのワールド位置をGPU用データに書き込む 
		Matrix4x4 cameraWorldMatrix = MyMath::Inverse(viewMatrix);
		cameraData->worldPosition = {
			cameraWorldMatrix.m[3][0],
			cameraWorldMatrix.m[3][1],
			cameraWorldMatrix.m[3][2]
		};

		// Fenceの行列更新  
		Matrix4x4 fenceWorldMatrix = MyMath::MakeAffineMatrix(fenceTransform.scale, fenceTransform.rotate, fenceTransform.translate);
		Matrix4x4 fenceWvpMatrix = MyMath::Multiply(fenceWorldMatrix, MyMath::Multiply(viewMatrix, projectionMatrix));

		// フィールドの行列更新
		Matrix4x4 terrainWorldMatrix = MyMath::Multiply(
			terrainModel.rootNode.localMatrix,                                                                    // 1つ目：Nodeの行列
			MyMath::MakeAffineMatrix(terrainTransform.scale, terrainTransform.rotate, terrainTransform.translate) // 2つ目：今までのWorld行列
		);
		Matrix4x4 terrainWvpMatrix = MyMath::Multiply(terrainWorldMatrix, MyMath::Multiply(viewMatrix, projectionMatrix));

		terrainWvpData->World = terrainWorldMatrix;
		terrainWvpData->WVP = terrainWvpMatrix;
		terrainWvpData->WorldInverseTranspose = MyMath::Transpose(MyMath::Inverse(terrainWorldMatrix));

		fenceWvpData->World = fenceWorldMatrix;
		fenceWvpData->WVP = fenceWvpMatrix;
		fenceWvpData->WorldInverseTranspose = MyMath::Transpose(MyMath::Inverse(fenceWorldMatrix));

		// ビルボード行列（カメラの回転だけを取り出す） 
		Matrix4x4 billboardMatrix = MyMath::Identity();
		if (useBillboard) {
			billboardMatrix = MyMath::Inverse(viewMatrix); // = カメラ行列
			billboardMatrix.m[3][0] = 0.0f; // 平行移動成分はいらない
			billboardMatrix.m[3][1] = 0.0f;
			billboardMatrix.m[3][2] = 0.0f;
		}

		// Emitterの更新：時間がたつと、自動でParticleを発生させる
		if (isUpdate && useAutoEmit) {
			if (isUpdate) {
				emitter.frequencyTime += kDeltaTime;                                 // 時刻を進める 
				if (emitter.frequency <= emitter.frequencyTime) {                    // 頻度より大きければ発生
					particles.splice(particles.end(), Emit(emitter, randomEngine));
					emitter.frequencyTime -= emitter.frequency;                      // 余計に過ぎた時間も加味して頻度計算する
				}
			}
		}

		// Instancing用：生きている Particle だけを更新して書き込む
		uint32_t numInstance = 0;
		uint32_t shapeInstanceCount[kNumShapes] = {};
		for (std::list<Particle>::iterator particleIterator = particles.begin();
			particleIterator != particles.end(); ) {

			// 寿命が来たら、listから消す（戻り値が次のイテレータ
			if ((*particleIterator).lifeTime <= (*particleIterator).currentTime) {
				particleIterator = particles.erase(particleIterator);
				continue;
			}

			// Fieldの範囲内のParticleには加速度を適用する
			if (useAccelerationField) { // フィールドON/OFF
				if (IsCollision(accelerationField.area, (*particleIterator).transform.translate)) {
					(*particleIterator).velocity.x += accelerationField.acceleration.x * kDeltaTime;
					(*particleIterator).velocity.y += accelerationField.acceleration.y * kDeltaTime;
					(*particleIterator).velocity.z += accelerationField.acceleration.z * kDeltaTime;
				}
			}


			// 時間を進める
			if (isUpdate) {
				(*particleIterator).currentTime += kDeltaTime;

				// 回転を進める 
				(*particleIterator).transform.rotate.x += (*particleIterator).angularVelocity.x * kDeltaTime;
				(*particleIterator).transform.rotate.y += (*particleIterator).angularVelocity.y * kDeltaTime;
				(*particleIterator).transform.rotate.z += (*particleIterator).angularVelocity.z * kDeltaTime;
			}

			// 寿命の進み具合（0.0〜1.0）
			float t = (*particleIterator).currentTime / (*particleIterator).lifeTime;
			if (t > 1.0f) { t = 1.0f; }

			// 前半は広がる、後半はそれを逆再生して戻る
			float p = 0.0f;
			if (t < 0.5f) {
				p = EaseOutCubic(t / 0.5f);          // 0 → 1
			}
			else {
				p = EaseOutCubic((1.0f - t) / 0.5f); // 1 → 0
			}

			// 位置 = 中心 + ずれ × 進み具合
			(*particleIterator).transform.translate = {
				(*particleIterator).startPosition.x + (*particleIterator).targetOffset.x * p,
				(*particleIterator).startPosition.y + (*particleIterator).targetOffset.y * p,
				(*particleIterator).startPosition.z + (*particleIterator).targetOffset.z * p };

			uint32_t shape = (*particleIterator).shapeIndex;
			if (shapeInstanceCount[shape] < kNumMaxInstances) {

				// 書き込む場所 = その形の区切りの先頭 + その形の何個目か
				uint32_t writeIndex = shape * kNumMaxInstances + shapeInstanceCount[shape];
				float s = 0.0f;
				if (t < 0.5f) {
					s = EaseOutBack(t / 0.5f);
				}
				else {
					s = EaseOutBack((1.0f - t) / 0.5f);  // 逆再生でしゅっと縮む 
				}

				// 元の大きさに倍率を掛ける
				Vector3 easedScale = {
					(*particleIterator).transform.scale.x * s,
					(*particleIterator).transform.scale.y * s,
					(*particleIterator).transform.scale.z * s };

				Matrix4x4 scaleMatrix = MyMath::MakeAffineMatrix(
					easedScale, (*particleIterator).transform.rotate, { 0.0f, 0.0f, 0.0f });

				Matrix4x4 translateMatrix = MyMath::MakeAffineMatrix(
					{ 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, (*particleIterator).transform.translate);
				Matrix4x4 worldMatrix = MyMath::Multiply(scaleMatrix, MyMath::Multiply(billboardMatrix, translateMatrix));
				Matrix4x4 worldViewProjectionMatrix =
					MyMath::Multiply(worldMatrix, MyMath::Multiply(viewMatrix, projectionMatrix));

				instancingData[writeIndex].WVP = worldViewProjectionMatrix;
				instancingData[writeIndex].World = worldMatrix;
				instancingData[writeIndex].color = (*particleIterator).color;

				float alpha = 1.0f - ((*particleIterator).currentTime / (*particleIterator).lifeTime);
				instancingData[writeIndex].color.w = alpha;

				++shapeInstanceCount[shape];
			}
			++particleIterator; // ifの外
		}

		// =====================================
		// 描画前処理
		// =====================================
		dxCommon->PreDraw();

		// =================================
		// 球の描画(Object3d用パイプライン) 
		// =================================
		commandList->SetGraphicsRootSignature(rootSignature.Get());
		commandList->SetPipelineState(graphicsPipelineStates[kBlendModeNone].Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		commandList->IASetVertexBuffers(0, 1, &sphereVertexBufferView);
		commandList->IASetIndexBuffer(&sphereIndexBufferView);

		commandList->SetGraphicsRootConstantBufferView(0, fenceWvpResource->GetGPUVirtualAddress());
		commandList->SetGraphicsRootConstantBufferView(1, materialResourceFence->GetGPUVirtualAddress());
		commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU2);
		commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
		commandList->SetGraphicsRootConstantBufferView(4, cameraResource->GetGPUVirtualAddress());
		commandList->SetGraphicsRootConstantBufferView(5, pointLightResource->GetGPUVirtualAddress());
		commandList->SetGraphicsRootConstantBufferView(6, spotLightResource->GetGPUVirtualAddress());

		// commandList->DrawIndexedInstanced(kSphereIndexCount, 1, 0, 0, 0);


		// =================================
		// フィールドの描画
		// =================================
		commandList->SetGraphicsRootConstantBufferView(0, terrainWvpResource->GetGPUVirtualAddress());
		commandList->SetGraphicsRootConstantBufferView(1, materialResourceTerrain->GetGPUVirtualAddress());
		commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU3);

		//for (const auto& mesh : terrainModel.meshes) {
		//	commandList->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
		//	commandList->DrawInstanced(static_cast<UINT>(mesh.vertices.size()), 1, 0, 0);
		//}

		// 共通設定
		commandList->SetGraphicsRootSignature(particleRootSignature.Get());
		commandList->SetPipelineState(particlePipelineState.Get());

		// =================================  
		// 3D描画 
		// =================================    
		// トポロジ
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// [1] Material (PS / b0)
		commandList->SetGraphicsRootConstantBufferView(1, materialResourceParticle->GetGPUVirtualAddress());
		// [2] Texture (PS / t0)
		commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);

		// 形ごとに描画する
		for (uint32_t shape = 0; shape < kNumShapes; ++shape) {
			if (shapeInstanceCount[shape] == 0) { continue; } // その形が1つもなければ飛ばす 

			commandList->SetGraphicsRootDescriptorTable(0, shapeSrvHandleGPU[shape]);

			for (const auto& mesh : particleModels[shape].meshes) {
				commandList->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
				commandList->DrawInstanced(static_cast<UINT>(mesh.vertices.size()), shapeInstanceCount[shape], 0, 0);
			}
		}


#ifdef USE_IMGUI
		ImGui::Render();
		ID3D12DescriptorHeap* descriptorHeaps[] = { dxCommon->GetSrvDescriptorHeap() };
		commandList->SetDescriptorHeaps(1, descriptorHeaps);
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
#endif

		// =====================================
		// 描画後処理
		// =====================================
		dxCommon->PostDraw();

		// 前フレームのキー状態を保存
		std::memcpy(keyPre, key, sizeof(key));

	}



	// ----- 解放処理 -----
	xAudio2.Reset();              // XAudio2インスタンス解放 
	//SoundUnload(&soundData1);     // 音声データ解放

	// 入力解放
	delete input;
	// DirectXの解放
	delete dxCommon;
	// WindowsAPIの終了処理
	winApp->Finalize();
	// WindowsAPI
	delete winApp;


	CoUninitialize();


	return 0;
};