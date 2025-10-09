
// /app/main.cpp（最終形）
//#include <Windows.h>
//#include "AppMain.h"
//
//
//int WINAPI WinMain(HINSTANCE h, HINSTANCE, LPSTR, int) {
//    AppMain app;
//    if (!app.Initialize(h)) return -1;
//    return app.Run();
//}



#define _WINSOCKAPI_
#define _WIN32_WINNT 0x0602 // Windows 8
#define ANCHOR_TEST

#include "DebugCamera.h"
#include "Matrix4x4.h"
#include "Vertex.h"
#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h" // ✅ これをmain.cppの先頭で追加
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "wrl.h"
#include <Windows.h>
#include <atomic>
#include <cassert>
#include <chrono>
#include <codecvt>
#include <cstdint>
#include <cstdio>
#include <d3d12.h>
#include <dbghelp.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <filesystem>
#include <format>
#include <fstream>
#include <locale>
#include <mutex>
#include <numbers>
#include <queue>
#include <sstream>
#include <string>
#include <strsafe.h>
#include <unordered_map>
#include <xaudio2.h>
#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定
#include <dinput.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#include <mferror.h>
#include <mfobjects.h>
#include <mftransform.h>
#include <comdef.h>
#include <map> 
#include "CameraCapture.h"
#include "ColorConverter.h"
#include "CustomByteStream.h"
#include "H264Encoder.h"
#include "MfH264Receiver.h"
#include "NetworkManager.h"
#include "TextureHelper.h"
#include "TextureUploader.h"
#include "UdpByteStream.h"
#include "NalUtils.h"

#include "./lib/Receiver/FrameProto.h"
#include "./lib/Receiver/startTcpReceiverRgb32.h"
#include "./platform/Window.h"
#include "core/Device.h"
#include "graphics/SwapChain.h"
#include "core/CommandListPool.h"
#include "core/DescriptorHeap.h"

// …WSAStartup はどこかの初期化で一度だけ呼んでおいてね…
/*
WSADATA wsa;
WSAStartup(MAKEWORD(2,2), &wsa);
*/

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfuuid.lib")

using namespace Microsoft::WRL;




//extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd,
//	UINT msg,
//	WPARAM wParam,
//	LPARAM lParam);
//
//LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
//	// ImGui が処理したメッセージなら、アプリ側の処理は不要
//	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
//		return true;
//	}
//
//	// メッセージに応じてゲーム固有の処理
//	switch (msg) {
//
//		// ウィンドウが破棄された
//	case WM_DESTROY:
//
//		// OSに対して,アプリの終了を伝える
//		PostQuitMessage(0);
//		return 0;
//	}
//
//	// 標準のメッセージ処理を行う
//	return DefWindowProc(hwnd, msg, wparam, lparam);
//}

typedef struct D3D12_CPU_DESCRIPTION_HANDLE {
	SIZE_T ptr;
} D3D12_CPU_DESCRIPTION_HANDLE;

struct SphereMeshData {
	ComPtr<ID3D12Resource> vertexResource;
	D3D12_VERTEX_BUFFER_VIEW vbv;
	ComPtr<ID3D12Resource> cbvResource;
	TransformationMatrix* mappedCBV;
	UINT vertexCount;
};

// MaterialData構造体
struct MaterialData {
	std::string textureFilePath;
};

// ModelData構造体
struct ModelData {
	std::vector<VertexData> vertices;
	MaterialData material;
};

// チャンクヘッダ
struct ChunkHeader {
	char id[4];   // チャンク毎のID
	int32_t size; // チャンクサイズ
};

// RIFFヘッダチャンク
struct RiffHeader {
	ChunkHeader chunk; // "RIFF"
	char type[4];      // "WAVE"
};

// FMTチャンク
struct FormatChunk {
	ChunkHeader chunk; // "fmt "
	WAVEFORMATEX fmt;  // 波形フォーマット
};

// 音声データ
struct SoundData {

	// 波形フォーマット
	WAVEFORMATEX wfex;

	// バッファの先頭アドレス
	BYTE* pBuffer;

	// バッファのサイズ
	unsigned int bufferSize;
};

SoundData SoundLoadWave(const char* filename) {
	// HRESULT result;

	// ファイルオープン
	//
	//  ファイル入力ストリームのインスタンス
	std::ifstream file;

	// .wavファイルをバイナリモードで開く
	file.open(filename, std::ios_base::binary);

	// ファイルオープン失敗を検出する
	assert(file.is_open());

	//.wav データ読み込み
	// RIFFヘッダーの読み込み
	RiffHeader riff;
	file.read((char*)&riff, sizeof(riff));

	// ファイルがRIFFかチェック
	if (strncmp(riff.chunk.id, "RIFF", 4) != 0) {
		assert(0);
	}

	// タイプがWAVEかチェック
	if (strncmp(riff.type, "WAVE", 4) != 0) {
		assert(0);
	}

	// Formatチャンクの読み込み
	FormatChunk format = {};

	// チャンクヘッダーの確認
	file.read((char*)&format, sizeof(ChunkHeader));
	if (strncmp(format.chunk.id, "fmt ", 4) != 0) {
		assert(0);
	}

	// チャンク本体の読み込み
	assert(format.chunk.size <= sizeof(format.fmt));
	file.read((char*)&format.fmt, format.chunk.size);

	// Dataチャンクの読み込み
	ChunkHeader data;
	file.read((char*)&data, sizeof(data));

	// JUNKチャンクを検出した場合
	if (strncmp(data.id, "JUNK", 4) == 0) {

		// 読み取り位置をJUNKチャンクの終わりまで進める
		file.seekg(data.size, std::ios_base::cur);

		// 再読み込み
		file.read((char*)&data, sizeof(data));
	}

	if (strncmp(data.id, "data", 4) != 0) {
		assert(0);
	}

	// Dataチャンクのデータ部（波形データ）の読み込み
	char* pBuffer = new char[data.size];
	file.read(pBuffer, data.size);

	// ファイルクローズ
	//  Waveファイルを閉じる
	file.close();

	// return する為の音声データ
	SoundData soundData = {};

	soundData.wfex = format.fmt;
	soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
	soundData.bufferSize = data.size;

	return soundData;
}

// 音声データ解放
void SoundUnload(SoundData* soundData) {

	// バッファのメモリを解放
	delete[] soundData->pBuffer;

	soundData->pBuffer = 0;
	soundData->bufferSize = 0;
	soundData->wfex = {};
}

// 音声再生
void SoundPlayWave(IXAudio2* xAudio2, const SoundData& soundData) {
	HRESULT result;

	// 波形フォーマットを元にSourceVoiceの生成
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

MaterialData LoadMaterialTemplateFile(const std::string& directoryPath,
	const std::string& filename);

// Objファイルを読む関数
ModelData LoadObjFile(const std::string& directoryPath,
	const std::string& filename) {
	// 1. 中で必要となる変数の宣言
	ModelData modelData;            // 構築するModelData
	std::vector<Vector4> positions; // 位置
	std::vector<Vector3> normals;   // 法線
	std::vector<Vector2> texcoords; // テクスチャ座標
	std::string line; // ファイルから読んだ1行を格納するもの

	// 2. ファイルを開く
	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
	assert(file.is_open()); // とりあえず開けなかったら止める

	// 3. 実際にファイルを読み、ModelDataを構築していく
	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);

		// 先頭の識別子を読む
		s >> identifier;

		// identifier に応じた処理

		// 頂点情報を読む
		if (identifier == "v") {
			Vector4 position;

			s >> position.x >> position.y >> position.z;
			position.w = 1.0f;
			position.x *= -1.0f;
			positions.push_back(position);

		}
		else if (identifier == "vt") {
			Vector2 texcoord;

			s >> texcoord.x >> texcoord.y;
			texcoord.y = 1.0f - texcoord.y;
			texcoords.push_back(texcoord);

		}
		else if (identifier == "vn") {
			Vector3 normal;

			s >> normal.x >> normal.y >> normal.z;
			normal.x *= -1.0f;
			normals.push_back(normal);

		}
		else if (identifier == "f") {

			VertexData triangle[3];

			// 面は三角形限定。その他は未対応
			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
				std::string vertexDefinition;
				s >> vertexDefinition;

				// 頂点の要素へのIndexは「位置/UV/法線」で格納されているので、分割して取得する
				std::istringstream w(vertexDefinition);
				uint32_t elementIndices[3];

				for (int32_t element = 0; element < 3; ++element) {
					std::string index;

					// 区切りでインデックスを読んでいく
					std::getline(w, index, '/');
					elementIndices[element] = std::stoi(index);
				}

				// 要素へのIndexから、実際の要素の値を取得して、頂点を構築する
				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcoord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];

				triangle[faceVertex] = { position, texcoord, normal };
			}

			// 頂点順を反時計回りに変更（CCW）
			modelData.vertices.push_back(triangle[2]);
			modelData.vertices.push_back(triangle[1]);
			modelData.vertices.push_back(triangle[0]);

		}
		else if (identifier == "mtllib") {
			// materialTemplateLibraryファイルの名前を取得する
			std::string materialFilename;
			s >> materialFilename;

			// 基本的にobjファイルと同一階層にmtlは存在させるので、ディレクトリ名とファイル名を渡す
			modelData.material =
				LoadMaterialTemplateFile(directoryPath, materialFilename);
		}
	}

	// 4. ModelDataを返す
	return modelData;
}

MaterialData LoadMaterialTemplateFile(const std::string& directoryPath,
	const std::string& filename) {

	// 1. 中で必要となる変数の宣言
	MaterialData materialData; // 構築するMaterialData
	std::string line; // ファイルから読んだ1行を格納するもの

	// 2. ファイルを開く
	std::ifstream file(directoryPath + "/" + filename);

	// とりあえず開けなかったら止める
	assert(file.is_open());

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

	// 4. MaterialData を返す
	return materialData;
}

// main.cpp の先頭付近、Log()関数などの後ろに以下を追加：
ComPtr<ID3D12Resource> CreateBufferResource(ComPtr<ID3D12Device> device,
	size_t sizeInBytes);

void Log(const std::wstring& message) { OutputDebugStringW(message.c_str()); }

void Log(std::ostream& os, const std::string& message) {
	os << message << std::endl;
	OutputDebugStringA(message.c_str());
}

void Log(const char* message) { OutputDebugStringA(message); }

void Log(const std::string& message) { OutputDebugStringA(message.c_str()); }

std::string ConvertString(const std::wstring& wstr) {

	if (wstr.empty())
		return "";
	int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0,
		nullptr, nullptr);
	std::string result(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], sizeNeeded,
		nullptr, nullptr);

	// null終端を除去
	result.pop_back();
	return result;
}

std::wstring ConvertString(const std::string& str) {

	if (str.empty()) {
		return L"";
	}

	int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
	std::wstring result(sizeNeeded, 0);
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], sizeNeeded);
	result.pop_back(); // null終端を除去
	return result;
}

IDxcBlob* CompileShader(
	// CompilerするShaderファイルへのパス
	const std::wstring& filePath,
	// Compilerに使用するProfile
	const wchar_t* profile,
	// 初期化で生成したものを3つ
	IDxcUtils* dxcUtils, IDxcCompiler3* dxcCompiler,
	IDxcIncludeHandler* includeHandler) {
	// ここに中身を書いていく
	// 1. .hlslファイルを読む
	// これからシェーダーをコンパイルする旨をログに出す
	Log(ConvertString(std::format(L"Begin CompileShader, path:{}, profile:{}\n",
		filePath, profile)));

	// hlslファイルを読む
	IDxcBlobEncoding* shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);

	// 読めなかったら止める
	assert(SUCCEEDED(hr));

	// 読み込んだファイルの内容を設定する
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8; // UTF8の文字コードであることを通知

	// 2. Compileする
	LPCWSTR arguments[] = {
		filePath.c_str(), // コンパイル対象の hlsl ファイル名
		L"-E",
		L"main", // エントリーポイントの指定。基本的に main 以外にはしない
		L"-T",
		profile, // ShaderProfile の設定
		L"-Zi",
		L"-Qembed_debug", // デバッグ用の情報を埋め込む
		L"-Od",           // 最適化を外しておく
		L"-Zpr",          // メモリレイアウトは行優先
	};

	// 実際に Shader をコンパイルする
	IDxcResult* shaderResult = nullptr;
	hr = dxcCompiler->Compile(
		&shaderSourceBuffer,        // 読み込んだファイル
		arguments,                  // コンパイルオプション
		_countof(arguments),        // コンパイルオプションの数
		includeHandler,             // include が含まれた時の対応
		IID_PPV_ARGS(&shaderResult) // コンパイル結果を受け取る
	);

	// コンパイルエラーではなく、dxc が起動できたかどうかをチェック
	assert(SUCCEEDED(hr));

	// 3. 警告・エラーが出ていないか確認する
	// 警告・エラーが出てたらログに出して止める
	IDxcBlobUtf8* shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);

	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Log(shaderError->GetStringPointer());
		// 警告・エラーダメゼッタイ
		assert(false);
	}

	// 4. Compile結果を受け取って返す
	// コンパイル結果から実行用のバイナリ部分を取得
	IDxcBlob* shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob),
		nullptr);
	assert(SUCCEEDED(hr));

	// 成功したログを出す
	Log(ConvertString(std::format(L"Compile Succeeded, path:{}, profile:{}\n",
		filePath, profile)));

	// もう使わないリソースを解放
	shaderSource->Release();
	shaderResult->Release();

	// 実行用のバイナリを返却
	return shaderBlob;
}

//**************************
// CrashHandlerの登録
//**************************
static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception) {
	// 時刻を取得して、時刻を名前に入れたファイルを作成。Dumpsディレクトリ以下に出力
	SYSTEMTIME time;
	GetLocalTime(&time);
	wchar_t filePath[MAX_PATH] = { 0 };
	CreateDirectory(L"./Dumps", nullptr);
	StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp",
		time.wYear, time.wMonth, time.wDay, time.wHour,
		time.wMinute);
	HANDLE dumpFileHandle =
		CreateFile(filePath, GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

	// processId(このexeのid)とクラッシュ(例外)の発生したthreadIdを取得
	DWORD processId = GetCurrentProcessId();
	DWORD threadId = GetCurrentThreadId();

	// 設定情報を入力
	MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{ 0 };
	minidumpInformation.ThreadId = threadId;
	minidumpInformation.ExceptionPointers = exception;
	minidumpInformation.ClientPointers = TRUE;

	// Dumpを出力。MiniDumpNormalは最低限の情報を出力するフラグ
	MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle,
		MiniDumpNormal, &minidumpInformation, nullptr, nullptr);

	// 他に関連付けられているSEH例外ハンドルがあれば実行。通常はプロセスを終了する
	return EXCEPTION_EXECUTE_HANDLER;
}

/// <summary>
/// 任意のバイトサイズの Upload バッファリソースを作成する
/// </summary>
/// <param name="device">ID3D12Device*</param>
/// <param name="sizeInBytes">作成したいバッファのバイトサイズ</param>
/// <returns>ID3D12Resource*</returns>
ComPtr<ID3D12Resource> CreateBufferResource(ComPtr<ID3D12Device> device,
	size_t sizeInBytes) {
	// ヒープ設定（Upload 用）
	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

	// リソースの記述（バッファ用）
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeInBytes;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// リソース作成
	ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));

	return resource;
}

ComPtr<ID3D12DescriptorHeap>
CreateDescriptorHeap(ComPtr<ID3D12Device> device,
	D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors,
	bool shaderVisible) {

	ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
	descriptorHeapDesc.Type = heapType;
	descriptorHeapDesc.NumDescriptors = numDescriptors;
	descriptorHeapDesc.Flags = shaderVisible
		? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
		: D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc,
		IID_PPV_ARGS(&descriptorHeap));
	assert(SUCCEEDED(hr));

	return descriptorHeap;
}

DirectX::ScratchImage LoadTexture(const std::string& filePath) {

	// テクスチャファイルを読んでプログラムで扱えるようにする
	DirectX::ScratchImage image{};
	std::wstring filePathW = ConvertString(filePath);
	HRESULT hr = DirectX::LoadFromWICFile(
		filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	// ミップマップの作成
	DirectX::ScratchImage mipImages{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(),
		image.GetMetadata(), DirectX::TEX_FILTER_SRGB,
		0, mipImages);
	assert(SUCCEEDED(hr));

	// ミップマップ付きのデータを返す
	return mipImages;
}

ComPtr<ID3D12Resource>
CreateTextureResource(ComPtr<ID3D12Device> device,
	const DirectX::TexMetadata& metadata) {

	// 1. metadataを基にResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width);           // Textureの幅
	resourceDesc.Height = UINT(metadata.height);         // Textureの高さ
	resourceDesc.MipLevels = UINT16(metadata.mipLevels); // mipmapの数
	resourceDesc.DepthOrArraySize =
		UINT16(metadata.arraySize); // 奥行き or 配列Textureの配列数
	resourceDesc.Format = metadata.format; // TextureのFormat
	resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。固定。
	resourceDesc.Dimension =
		D3D12_RESOURCE_DIMENSION(metadata.dimension); // Textureの次元数。

	// 2. 利用するHeapの設定
	// 利用するHeapの設定。非常に特殊な運用。02_04exで一般的なケース版がある
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM; // 細かい設定を行う
	heapProperties.CPUPageProperty =
		D3D12_CPU_PAGE_PROPERTY_WRITE_BACK; // WriteBackポリシーでCPUアクセス可能
	heapProperties.MemoryPoolPreference =
		D3D12_MEMORY_POOL_L0; // プロセッサの近くに配置

	// 3. Resourceを生成する
	// Resourceの生成
	ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,      // Heapの設定
		D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定。特になし。
		&resourceDesc,        // Resourceの設定
		D3D12_RESOURCE_STATE_GENERIC_READ, // 初回のResourceState。Textureは基本読むだけ
		nullptr,                // Clear最適値。使わないのでnullptr
		IID_PPV_ARGS(&resource) // 作成するResourceポインタへのポインタ
	);

	assert(SUCCEEDED(hr));
	return resource;
}

void UploadTextureData(ComPtr<ID3D12Resource> texture,
	const DirectX::ScratchImage& mipImages) {
	// Meta情報を取得
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();

	// 全MipMapについて
	for (size_t mipLevel = 0; mipLevel < metadata.mipLevels; ++mipLevel) {

		// MipMapLevelを指定して各Imageを取得
		const DirectX::Image* img = mipImages.GetImage(mipLevel, 0, 0);

		// Textureに転送
		HRESULT hr = texture->WriteToSubresource(
			UINT(mipLevel),      // 書き込むmipレベル
			nullptr,             // 全領域へコピー
			img->pixels,         // 元データの先頭
			UINT(img->rowPitch), // 1ラインのバイト数
			UINT(img->slicePitch) // 1スライスのサイズ（=画像全体のバイト数）
		);
		assert(SUCCEEDED(hr));
	}
}

ComPtr<ID3D12Resource>
CreateDepthStencilTextureResource(ComPtr<ID3D12Device> device, int32_t width,
	int32_t height) {

	// 生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;        // Textureの幅
	resourceDesc.Height = height;      // Textureの高さ
	resourceDesc.MipLevels = 1;        // mipmapの数
	resourceDesc.DepthOrArraySize = 1; // 奥行き or 配列Textureの配列数
	resourceDesc.Format =
		DXGI_FORMAT_D24_UNORM_S8_UINT; // DepthStencilとして利用可能なフォーマット
	resourceDesc.SampleDesc.Count = 1; // サンプルカウント。1固定。
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2次元
	resourceDesc.Flags =
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // DepthStencilとして使う通知

	// 利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // VRAM上に作る

	// 深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f; // 1.0f（最大値）でクリア
	depthClearValue.Format =
		DXGI_FORMAT_D24_UNORM_S8_UINT; // フォーマット。Resourceと合わせる

	// Resourceの生成
	ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,      // Heapの設定
		D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定。特になし。
		&resourceDesc,        // Resourceの設定
		D3D12_RESOURCE_STATE_DEPTH_WRITE, // 深度値を書き込む状態にしておく
		&depthClearValue,                 // Clear最適値
		IID_PPV_ARGS(&resource)); // 作成するResourceポインタへのポインタ
	assert(SUCCEEDED(hr));
	return resource;
}

// CPU 側のディスクリプタハンドルを取得
inline D3D12_CPU_DESCRIPTOR_HANDLE
GetCPUDescriptorHandle(ComPtr<ID3D12DescriptorHeap> descriptorHeap,
	uint32_t descriptorSize, uint32_t index) {

	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU =
		descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += descriptorSize * index;
	return handleCPU;
}

// GPU 側のディスクリプタハンドルを取得
inline D3D12_GPU_DESCRIPTOR_HANDLE
GetGPUDescriptorHandle(ComPtr<ID3D12DescriptorHeap> descriptorHeap,
	uint32_t descriptorSize, uint32_t index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU =
		descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += descriptorSize * index;
	return handleGPU;
}





//
// ===== 共有コンテキスト（A〜Dの状態を保持） =====
struct CpuTransferCtx {
	// A) D3D11 Staging 再利用用
	ComPtr<ID3D11Texture2D> stagingTex;
	D3D11_TEXTURE2D_DESC    stagingDesc{}; // 現状を保持

	// D) STREAM_CHANGE 対応（出力側の変化に追従）
	D3D12_RESOURCE_DESC     lastDstDesc{}; // 直近のdstRGBTex12のDesc

	// B/C) D3D12 Upload リング & フェンス
	static constexpr UINT   kRing = 3;
	ComPtr<ID3D12Resource>  upload[kRing];
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{}; // CopyableFootprints
	UINT                    numRows = 0;
	UINT64                  rowSize = 0;
	UINT64                  totalBytes = 0;
	UINT64                  fenceValue[kRing]{};
	ComPtr<ID3D12Fence>     fence;
	HANDLE                  fenceEvent = nullptr;
	UINT64                  nextFence = 1;
	UINT                    ringIndex = 0;

	// 内部専用：即席コピー用の D3D12 コマンド（他と干渉しない）
	ComPtr<ID3D12CommandAllocator> cmdAlloc;
	ComPtr<ID3D12GraphicsCommandList> cmdList;
};


struct D3DResourceLeakChecker {
	~D3DResourceLeakChecker() {

		// リソースリークチェック
		Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
			debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
		}
	}
};

void SendFrameFragmentedUDP(const std::vector<BYTE>& frameData,
	uint16_t frameID, SOCKET sock,
	const sockaddr_in& destAddr) {
	const size_t maxPayloadSize = 1400; // UDPパケットの最大サイズ（ヘッダ除く）
	size_t totalChunks = (frameData.size() + maxPayloadSize - 1) / maxPayloadSize;

	for (size_t chunkID = 0; chunkID < totalChunks; ++chunkID) {
		size_t offset = chunkID * maxPayloadSize;
		size_t chunkSize = (std::min)(maxPayloadSize, frameData.size() - offset);

		// 独自ヘッダを作成（6バイト）
		uint8_t header[6];
		header[0] = (frameID >> 8) & 0xFF;
		header[1] = frameID & 0xFF;
		header[2] = static_cast<uint8_t>(chunkID);
		header[3] = static_cast<uint8_t>(totalChunks);
		header[4] = (chunkID + 1 == totalChunks) ? 1 : 0; // isLastChunk
		header[5] = 1;                                    // version/reserved

		// パケット作成
		std::vector<BYTE> packet;
		packet.insert(packet.end(), header, header + sizeof(header));
		packet.insert(packet.end(), frameData.begin() + offset,
			frameData.begin() + offset + chunkSize);

		// UDP送信
		int sentBytes =
			sendto(sock, reinterpret_cast<const char*>(packet.data()),
				static_cast<int>(packet.size()), 0,
				reinterpret_cast<const sockaddr*>(&destAddr), sizeof(destAddr));

		if (sentBytes == SOCKET_ERROR) {
			char err[128];
			sprintf_s(err,
				"[ERROR] UDP failed: FrameID=%d Chunk=%zu/%zu WSAError=%d\n",
				frameID, chunkID, totalChunks, WSAGetLastError());
			OutputDebugStringA(err);
		}
		else {
			char msg[128];
			sprintf_s(msg, "[SEND] Frame %d - Chunk %zu/%zu - %d bytes\n", frameID,
				chunkID, totalChunks, sentBytes);
			OutputDebugStringA(msg);
		}
	}
}

// 時刻ユーティリティ
static inline long long NowNs() {
	return std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::steady_clock::now().time_since_epoch())
		.count();
}
static inline long long NsToMs(long long ns) { return ns / 1'000'000; }

// 監視メトリクス（超軽量）
struct RxMon {
	std::atomic<unsigned long long> pkts{ 0 }, bytes{ 0 };
	std::atomic<long long> last_ns{ 0 };
} g_rx;

struct AsmMon {
	std::atomic<unsigned long long> ok{ 0 }, timeout_drop{ 0 };
	std::atomic<long long> last_ok_ns{ 0 };
} g_asm;

struct ValMon {
	std::atomic<unsigned long long> valid{ 0 }, invalid_au{ 0 };
	std::atomic<long long> last_sps_ns{ 0 }, last_idr_ns{ 0 };
} g_val;

struct QMon {
	std::atomic<unsigned long long> enq{ 0 }, trim_drop{ 0 };
	std::atomic<int> qlen{ 0 };
} g_q;


//>>> ADD 2) グローバル or main() 冒頭近く：デコード/描画の進捗カウンタ
static UINT64 g_DecFrames = 0;      // ProcessOutput で増やす
static UINT64 g_CopyFrames = 0;     // NV12→D3D12 コピー成功で増やす
static UINT64 g_DrawFrames = 0;     // 実描画完了で増やす

//>>> ADD 3) Y平面用PS (HLSL)。既存のPSO差し替えが難しければ、
//           既存PSを一時的にこれに置き換えてビルドしてください。
//           (埋め込みリテラルでD3DCompileする場合は別途対応)
/*
// File: PS_YOnly.hlsl
Texture2D yTex : register(t0);
SamplerState samp : register(s0);
struct PSIn { float4 pos:SV_POSITION; float2 uv:TEXCOORD0; };
float4 PSMain(PSIn i) : SV_Target {
	float y = yTex.Sample(samp, i.uv).r; // 0..1
	return float4(y,y,y,1);
}
*/

//>>> ADD 4) NV12 テクスチャから Y 平面 SRV を作る関数
// nv12Tex: DXGI_FORMAT_NV12 の Default Heap リソース（復号を書き込む先）
// srvHeap: CPU 可視の SRV デスクリプタヒープ
// ySrvCpu: SRV を書き込む CPU ハンドル（空きスロット）
static void CreateYPlaneSRV(ID3D12Device* device,
	ID3D12Resource* nv12Tex,
	D3D12_CPU_DESCRIPTOR_HANDLE ySrvCpu)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC yDesc{};
	yDesc.Format = DXGI_FORMAT_R8_UNORM; // ★Y平面
	yDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	yDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	yDesc.Texture2D.MipLevels = 1;
	yDesc.Texture2D.MostDetailedMip = 0;
	yDesc.Texture2D.PlaneSlice = 0;     // ★Plane 0 = Y
	yDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	device->CreateShaderResourceView(nv12Tex, &yDesc, ySrvCpu);
}

//>>> ADD 5) NV12→Default へのコピーが終わってから描画に進むための同期（フェンス待ち）
static void WaitGPU(ID3D12CommandQueue* queue, ID3D12Fence* fence,
	UINT64& fenceValue, HANDLE fenceEvent)
{
	fenceValue++;
	queue->Signal(fence, fenceValue);
	fence->SetEventOnCompletion(fenceValue, fenceEvent);
	WaitForSingleObject(fenceEvent, INFINITE);
}

//>>> ADD 6) 1フレームの更新シーケンス例（メインループ内の描画前）：
// - ここでは、既に CPU 側で NV12 の Y/UV を UploadHeap に書いた後、
//   CopyTextureRegion で nv12Tex(Default) へコピーする処理がある前提。
// - その直後にフェンスを待ち、SRVを確実に使える状態にしてから描画へ進む。
static void AfterCopy_SyncAndLog(ID3D12CommandQueue* queue,
	ID3D12Fence* fence,
	UINT64& fenceValue,
	HANDLE fenceEvent)
{
	WaitGPU(queue, fence, fenceValue, fenceEvent);
	++g_CopyFrames;
	if ((g_CopyFrames % 30) == 0) {
		char buf[128];
		sprintf_s(buf, "[COPY] done frames=%llu\n", (unsigned long long)g_CopyFrames);
		/*DBG(buf);*/
	}
}

//>>> ADD 7) 実描画時：DescriptorTable に t0 (YのSRV) をセットして描く
// 注意: ここはあなたの既存の RootSignature / PSO / DescriptorTable の仕様に合わせてください。
// 典型例（RootParameter #0 が SRV レンジ, #1 が サンプラ の場合）
static void DrawYPlaneGrayscale(ID3D12GraphicsCommandList* cmd,
	ID3D12DescriptorHeap* srvHeap,
	D3D12_GPU_DESCRIPTOR_HANDLE ySrvGpu,
	D3D12_VIEWPORT vp,
	D3D12_RECT scissor)
{
	ID3D12DescriptorHeap* heaps[] = { srvHeap };
	cmd->SetDescriptorHeaps(1, heaps);
	cmd->RSSetViewports(1, &vp);
	cmd->RSSetScissorRects(1, &scissor);

	// ルートシグネチャ/PSOは YOnly 用に組んでおく（VS=既存のフルスクリーン矩形でOK、PS=PS_YOnly）
	// cmd->SetGraphicsRootSignature(g_RootSigYOnly.Get());
	// cmd->SetPipelineState(g_PsoYOnly.Get());

	// t0 に Y の SRV をバインド（例：rootParam 0 が SRV テーブル）
	cmd->SetGraphicsRootDescriptorTable(/*rootParamIndex=*/0, ySrvGpu);

	// フルスクリーン矩形などを描画
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	cmd->DrawInstanced(4, 1, 0, 0);

	++g_DrawFrames;
	if ((g_DrawFrames % 60) == 0) {
		char buf[128];
		sprintf_s(buf, "[DRAW] yplane frames=%llu\n", (unsigned long long)g_DrawFrames);
		/*DBG(buf);*/
	}
}


// TcpThread_MJPEG.cpp
// 目的: 既存の H.264 受信スレッド（再構成→FeedAccessUnit）を、MJPEG（JPEGフレーム）前提に置き換え。
// - TCPで分割されたJPEGフレーム（チャンク）を再構成
// - 完成したJPEGをキューへ積む（デコードは描画スレッド側）
// - 描画スレッド側で WIC で JPEG→RGBA32 をデコードし、D3D12 Upload→Default に転送
// 依存: <wincodec.h> (WIC)







// 使い方:
// 1) StartTcpThread_MJPEG(...) で受信開始（detachedせず保持推奨）
// 2) 毎フレーム: ResourceBarrier( defaultTex: PSR→COPY_DEST ),
//                ProcessOneMJPEGFrameToD3D12(...),
//                ResourceBarrier( defaultTex: COPY_DEST→PSR )
// 3) 送信側は各JPEGフレームをチャンク分割して上記ヘッダ付きで送る。

using FrameQ = std::shared_ptr<std::queue<std::vector<uint8_t>>>;
using MtxSP = std::shared_ptr<std::mutex>;
using CvSP = std::shared_ptr<std::condition_variable>;

static inline void dbg(const char* s) { OutputDebugStringA(s); }

static bool recv_exact(SOCKET s, uint8_t* dst, size_t needBytes, int timeoutMs = 500) {
	DWORD to = timeoutMs;
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&to, sizeof(to));
	size_t got = 0;
	int timeoutRetry = 0;

	while (got < needBytes) {
		int r = recv(s, reinterpret_cast<char*>(dst + got), (int)(needBytes - got), 0);
		if (r == 0) return false; // 切断
		if (r < 0) {
			int e = WSAGetLastError();
			if (e == WSAETIMEDOUT) {
				if (++timeoutRetry > 10) {
					OutputDebugStringA("[ERR] recv timeout retry limit reached\n");
					return false;
				}
				continue; // リトライ
			}
			return false;
		}
		got += (size_t)r;
		timeoutRetry = 0; // リセット
	}
	return true;
}


static bool recv_u32be(SOCKET s, uint32_t& outSize) {
	uint8_t sz[4];
	if (!recv_exact(s, sz, 4)) return false;
	outSize = (uint32_t(sz[0]) << 24) | (uint32_t(sz[1]) << 16) | (uint32_t(sz[2]) << 8) | uint32_t(sz[3]);
	return true;
}

// =============================
// 受信スレッド本体
// =============================
// 先頭に追加: フォーマット済みログの小ヘルパ
static void logf(const char* fmt, ...) {
	char buf[256];
	va_list ap; va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	OutputDebugStringA(buf);
}
//
//// TCPでRGB32を1フレームずつ受信し、frameQueueに格納する受信スレッド関数（magic+len ヘッダ対応 / 32B固定）
//std::thread startTcpReceiverRgb32(
//	std::shared_ptr<std::queue<std::vector<BYTE>>> frameQueue,
//	std::shared_ptr<std::mutex> frameQueueMtx,
//	std::shared_ptr<std::condition_variable> frameQueueCv,
//	std::atomic<bool>& stopRequested,
//	uint16_t listenPort,
//	int width,
//	int height
//) {
//	return std::thread([=, &stopRequested]() {
//		auto dbg = [](const char* s) { OutputDebugStringA(s); };
//
//		// ---- 定数 ----
//		constexpr size_t bytesPerPixel = 4;
//		constexpr size_t maxFrameBytes = (1ull << 30);
//		constexpr DWORD  recvTimeoutMs = 2000;
//		constexpr BOOL   enable = TRUE;
//
//		if (width <= 0 || height <= 0) { dbg("[ERROR] Invalid width/height\n"); return; }
//		const size_t expectedBytes = size_t(width) * size_t(height) * bytesPerPixel;
//		if (expectedBytes == 0 || expectedBytes > maxFrameBytes) {
//			dbg("[ERROR] expectedBytes is invalid or too large\n"); return;
//		}
//
//		// ---- ソケット ----
//		SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
//		if (listenSock == INVALID_SOCKET) { dbg("[ERROR] TCP socket creation failed\n"); return; }
//		setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&enable), sizeof(enable));
//
//		sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(listenPort); addr.sin_addr.s_addr = INADDR_ANY;
//		if (bind(listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
//			dbg("[ERROR] bind() failed\n"); closesocket(listenSock); return;
//		}
//		if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR) {
//			dbg("[ERROR] listen() failed\n"); closesocket(listenSock); return;
//		}
//		dbg("[INFO] Waiting for TCP client...\n");
//
//		SOCKET clientSock = accept(listenSock, nullptr, nullptr);
//		if (clientSock == INVALID_SOCKET) { dbg("[ERROR] accept() failed\n"); closesocket(listenSock); return; }
//		closesocket(listenSock);
//		dbg("[INFO] TCP client connected\n");
//
//		setsockopt(clientSock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&enable), sizeof(enable));
//		setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&recvTimeoutMs), sizeof(recvTimeoutMs));
//
//		// ---- 共有ヘッダ定義（送受で完全一致させる・32B固定）----
//#pragma pack(push,1)
//		struct FrameHdr {
//			uint8_t  magic[16];     // 16Bシグネチャ
//			uint32_t payloadBytes;  // ペイロード長（tight = w*h*4）
//			uint32_t frameId;       // 連番
//			uint32_t srcStride;     // 送信1行バイト数（tightなら w*4）
//			uint8_t  flags;         // bit0:BGRA(1)/RGBA(0), bit1:topDown(1)/bottomUp(0)
//			uint8_t  reserved[3];   // 32B境界合わせ
//		};
//#pragma pack(pop)
//		static_assert(sizeof(FrameHdr) == 32, "FrameHdr must be 32 bytes");
//
//		static const uint8_t FRAME_MAGIC16[16] =
//		{ 0xDE,0xAD,0xBE,0xEF,0xCA,0xFE,0xBA,0xBE,0x12,0x34,0x56,0x78,0x9A,0xBC,0xEF,0x01 };
//		constexpr size_t HDRLEN = sizeof(FrameHdr); // 32
//
//		// ---- 受信バッファ＆ヘルパ ----
//		std::vector<BYTE> sbuf; sbuf.reserve(expectedBytes * 2 + HDRLEN);
//
//		auto logf = [](const char* fmt, ...) {
//			char buf[256]; va_list ap; va_start(ap, fmt);
//			vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
//			OutputDebugStringA(buf);
//			};
//		auto find_magic = [&](const uint8_t* p, size_t n)->ptrdiff_t {
//			if (!p || n < 16) return -1;
//			for (size_t i = 0; i + 16 <= n; ++i)
//				if (memcmp(p + i, FRAME_MAGIC16, 16) == 0) return (ptrdiff_t)i;
//			return -1;
//			};
//
//		// ---- 受信ループ ----
//		while (!stopRequested) {
//			BYTE tmp[128 * 1024];
//			int ret = recv(clientSock, reinterpret_cast<char*>(tmp), sizeof(tmp), 0);
//			if (ret == 0) { dbg("[INFO] TCP connection closed by peer\n"); break; }
//			if (ret < 0) {
//				const int err = WSAGetLastError();
//				if (err == WSAETIMEDOUT || err == WSAEWOULDBLOCK) { if (stopRequested) break; continue; }
//				dbg("[ERROR] recv() failed\n"); break;
//			}
//			sbuf.insert(sbuf.end(), tmp, tmp + ret);
//
//			for (;;) {
//				if (stopRequested) break;
//
//				// 1) MAGIC 同期
//				ptrdiff_t mpos = find_magic(sbuf.data(), sbuf.size());
//				if (mpos < 0) {
//					if (sbuf.size() > (expectedBytes + HDRLEN) * 2) {
//						const size_t keep = expectedBytes + HDRLEN;
//						sbuf.erase(sbuf.begin(), sbuf.end() - (ptrdiff_t)keep);
//						logf("[TRIM] sbuf trimmed, now=%zu\n", sbuf.size());
//					}
//					break; // 追加受信へ
//				}
//				if (mpos > 0) sbuf.erase(sbuf.begin(), sbuf.begin() + mpos);
//
//				// 2) ヘッダ待ち
//				if (sbuf.size() < HDRLEN) break;
//
//				// 3) ヘッダ検証
//				FrameHdr hdr{}; memcpy(&hdr, sbuf.data(), HDRLEN);
//				if (memcmp(hdr.magic, FRAME_MAGIC16, 16) != 0) { sbuf.erase(sbuf.begin()); continue; }
//
//				const size_t pay = (size_t)hdr.payloadBytes;
//				if (pay == 0 || pay > maxFrameBytes) {
//					sbuf.erase(sbuf.begin()); logf("[WARN] invalid payloadBytes=%zu\n", pay); continue;
//				}
//#ifndef NDEBUG
//				if (hdr.srcStride != (uint32_t)width * 4) {
//					char m[128]; sprintf_s(m, "[RXWARN] srcStride=%u expected=%u\n", hdr.srcStride, width * 4); OutputDebugStringA(m);
//				}
//#endif
//				const size_t need = HDRLEN + pay;
//				if (sbuf.size() < need) break; // まだ足りない
//
//				// 4) ペイロード抽出
//				std::vector<BYTE> frame(
//					sbuf.begin() + (ptrdiff_t)HDRLEN,
//					sbuf.begin() + (ptrdiff_t)need
//				);
//				// 消費
//				sbuf.erase(sbuf.begin(), sbuf.begin() + (ptrdiff_t)need);
//
//#ifndef NDEBUG
//				// 起動直後だけヘッダ＆先頭ダンプ
//				static int s_dump = 0;
//				if (s_dump < 8) {
//					size_t z = 0, nz = 0; for (size_t i = 3; i < frame.size(); i += 4) (frame[i] == 0) ? ++z : ++nz;
//					char head[16 * 3 + 1] = {}; int p = 0;
//					for (int i = 0; i < 16 && i < (int)frame.size(); ++i) p += sprintf_s(head + p, sizeof(head) - p, "%02X ", frame[i]);
//					char m[256];
//					sprintf_s(m, "[RX] id=%u pay=%zu stride=%u flags=0x%02X head16:%s | A0=%zu A!=0=%zu\n",
//						hdr.frameId, pay, hdr.srcStride, hdr.flags, head, z, nz);
//					OutputDebugStringA(m); ++s_dump;
//				}
//#endif
//				// 5) キュー投入（過密ならドロップ）
//				{
//					std::lock_guard<std::mutex> lock(*frameQueueMtx);
//					constexpr size_t kMaxQueued = 3;
//					while (frameQueue->size() >= kMaxQueued) frameQueue->pop();
//					frameQueue->push(std::move(frame));
//				}
//				frameQueueCv->notify_one();
//			}
//		}
//
//		shutdown(clientSock, SD_BOTH);
//		closesocket(clientSock);
//		dbg("[INFO] TCP receiver thread exited\n");
//		});
//}




// === 2. DirectX12 テクスチャにRGB32をアップロード ===
// === 2. DirectX12 テクスチャにRGB32をアップロード ===
// === GrabFrameToD3D12_RGB: フレームを受信してアップロード用リソースに転送 ===
// フレームを取り出して UPLOAD Buffer を用意（行コピーはRowPitch=256アラインで）
// 修正版 GrabFrameToD3D12_RGB()
// - UploadBuffer を再利用
// - Fence によって GPU コピー完了を保証
// - CopyTextureRegion まで内部で完結
// - frameQueue が空のときは前回のテクスチャ内容を保持


// ==== 設定トグル ====
// 受信フレームの実体: PackRGB32Tight の出力（tight BGRA / top-down）を想定
#ifndef INPUT_IS_BGRA_TIGHT_TOPDOWN
#define INPUT_IS_BGRA_TIGHT_TOPDOWN 1
#endif

// 表示テクスチャのフォーマットに合わせる
// B8G8R8A8_UNORM を使っているなら 1（推奨）。R8G8B8A8_UNORM を使っているなら 0。
#ifndef TEXTURE_IS_B8G8R8A8
#define TEXTURE_IS_B8G8R8A8 1
#endif

// GrabFrameToD3D12_RGB: 受信フレーム(BGRA/4Bpp, top-down, tight)を GPU テクスチャへ転送
bool GrabFrameToD3D12_RGB(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	ID3D12CommandQueue* queue,
	ID3D12Fence* fence,
	HANDLE fenceEvent,
	uint64_t& fenceValue,
	ID3D12Resource* dstTex,
	UINT width,
	UINT height,
	std::shared_ptr<std::queue<std::vector<BYTE>>> frameQueue,
	std::shared_ptr<std::mutex> frameQueueMtx)
{
	static Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer = nullptr;
	static D3D12_PLACED_SUBRESOURCE_FOOTPRINT uploadFootprint{};
	static UINT64 uploadTotalBytes = 0;
	static bool hasEverCopied = false;

	// --- Upload バッファ見積もり＆確保（dstTex から Footprint を取る）---
	{
		const D3D12_RESOURCE_DESC texDesc = dstTex->GetDesc();
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};
		UINT   numRows = 0;
		UINT64 rowSize = 0;
		UINT64 total = 0;
		device->GetCopyableFootprints(&texDesc, 0, 1, 0, &fp, &numRows, &rowSize, &total);

		if (!uploadBuffer || uploadTotalBytes < total) {
			const CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
			const auto bufDesc = CD3DX12_RESOURCE_DESC::Buffer(total);
			Microsoft::WRL::ComPtr<ID3D12Resource> newBuf;
			if (FAILED(device->CreateCommittedResource(
				&heap, D3D12_HEAP_FLAG_NONE, &bufDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
				IID_PPV_ARGS(newBuf.ReleaseAndGetAddressOf())))) {
				return false;
			}
			uploadBuffer = newBuf;
			uploadTotalBytes = total;
		}
		uploadFootprint = fp;
	}

	// --- キューから最新フレームを1つ取り出す ---
	std::vector<BYTE> frame;
	{
		std::lock_guard<std::mutex> lock(*frameQueueMtx);
		if (!frameQueue->empty()) {
			frame = std::move(frameQueue->front());
			frameQueue->pop();
		}
	}
	if (frame.empty()) {
		return hasEverCopied ? true : false; // 初回だけ false で「まだ無い」
	}

	const UINT bytesPerPixel = 4;
	const UINT rowBytes = width * bytesPerPixel;
	const size_t expected = size_t(rowBytes) * size_t(height);
	if (frame.size() < expected) {
		// 受信サイズ不足（途中切れ等）
		return hasEverCopied ? true : false;
	}

	// --- GPU が前フレームを使い終わるのを待つ（単純同期）---
	if (fence->GetCompletedValue() < fenceValue) {
		fence->SetEventOnCompletion(fenceValue, fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);
	}

	// --- Upload へ行コピー（dst: Footprint.RowPitch / src: tight rowBytes）---
	uint8_t* mapped = nullptr;
	if (FAILED(uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mapped)))) {
		return false;
	}
	uint8_t* dstBase = mapped + uploadFootprint.Offset;
	const UINT dstPitch = uploadFootprint.Footprint.RowPitch;

	if (dstPitch < rowBytes) { // 256アライン想定
		uploadBuffer->Unmap(0, nullptr);
		return false;
	}

	// ======== ここから自動判定で R/B 入替を決定 ========
	const DXGI_FORMAT fmt = dstTex->GetDesc().Format;
	auto isBGRA = [](DXGI_FORMAT f) {
		switch (f) {
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8A8_TYPELESS: return true;
		default: return false;
		}
		};
	auto isRGBA = [](DXGI_FORMAT f) {
		switch (f) {
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_R8G8B8A8_TYPELESS: return true;
		default: return false;
		}
		};

	// 入力は PackRGB32Tight の出力＝BGRA（tight, top-down）
	const bool texIsBGRA = isBGRA(fmt);
	const bool texIsRGBA = isRGBA(fmt);
	const bool swapRB = texIsRGBA && !texIsBGRA; // テクスチャが RGBA なら入替（入力は BGRA）
	// ===============================================

	const uint8_t* srcBase = frame.data();
	for (UINT y = 0; y < height; ++y) {
		const uint8_t* srcRow = srcBase + size_t(y) * rowBytes;
		uint8_t* dstRow = dstBase + size_t(y) * dstPitch;

		if (swapRB) {
			// BGRA -> RGBA（R/Bスワップ）＋ α=FF
			for (UINT x = 0; x < rowBytes; x += 4) {
				uint8_t b = srcRow[x + 0];
				uint8_t g = srcRow[x + 1];
				uint8_t r = srcRow[x + 2];
				dstRow[x + 0] = r;
				dstRow[x + 1] = g;
				dstRow[x + 2] = b;
				dstRow[x + 3] = 0xFF;
			}
		}
		else {
			// 色順が同じ：そのままコピー＋ α=FF 上書き（黒化回避）
			memcpy(dstRow, srcRow, rowBytes);
			for (UINT x = 3; x < rowBytes; x += 4) dstRow[x] = 0xFF;
		}

		// RowPitch の余白は 0 埋め
		if (dstPitch > rowBytes) {
			memset(dstRow + rowBytes, 0, dstPitch - rowBytes);
		}
	}

	uploadBuffer->Unmap(0, nullptr);

	// --- Copy: Upload → DefaultHeap（記録のみ。Close/Execute は呼び出し側）---
	D3D12_TEXTURE_COPY_LOCATION dst{};
	dst.pResource = dstTex;
	dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dst.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION src{};
	src.pResource = uploadBuffer.Get();
	src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	src.PlacedFootprint = uploadFootprint;

	auto b1 = CD3DX12_RESOURCE_BARRIER::Transition(
		dstTex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
	cmdList->ResourceBarrier(1, &b1);

	cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

	auto b2 = CD3DX12_RESOURCE_BARRIER::Transition(
		dstTex, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	cmdList->ResourceBarrier(1, &b2);

	hasEverCopied = true;
	return true;
}







int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

	// comの初期化
	CoInitializeEx(0, COINIT_MULTITHREADED);

	D3DResourceLeakChecker leakCheck;

	// 誰も最速しなかった場合に(Unhandle),補足する関数を登録
	SetUnhandledExceptionFilter(ExportDump);

	std::filesystem::create_directory("logs");

	eng::platform::Window window;
	eng::platform::WindowDesc wd;
	wd.title = u"CG2WindowClass";
	wd.width = 1280;
	wd.height = 720;
	if (!window.Create(wd)) return -1;

	HWND hwnd = window.Handle();

	// DXGIファクトリーの生成
	ComPtr<IDXGIFactory7> dxgiFactory = nullptr;

	// 関数が成功したかどうかをSUCCEEDEDマクロで判別する
	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));

	// 初期化の根本的な部分でのエラーが出た場合、アサートにする
	assert(SUCCEEDED(hr));

	// 使用するアダプタ用の変数
	ComPtr<IDXGIAdapter4> useAdapter = nullptr;

	// 良い順にアダプタ
	for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(
		i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
		IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND;
		++i) {

		// アダプターの情報っを取得する
		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = useAdapter->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr));

		// ソフトウェアアダプタでなければ採用
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			// 採用したアダプタの情報をログに出力
			Log(std::format(L"Use Adapater:{}\n", adapterDesc.Description));
			break;
		}
		useAdapter = nullptr;
	}

	assert(useAdapter != nullptr);

	////**************************
	//// D3D12Deviceの生成
	////**************************

	core::Device dev;
	if (!dev.Initialize(/*enableDebugLayer=*/true)) { return -1; }

	////**************************
	//// コマンドキュー
	////**************************

	// 以降はこのポインタを使う
	Microsoft::WRL::ComPtr < ID3D12Device> device = dev.GetDevice();
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue = dev.GetCommandQueue();
	Microsoft::WRL::ComPtr<ID3D12Fence>        fence = dev.GetFence();



	// コマンドアロケータを生成する
	ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr;
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&commandAllocator));
	assert(SUCCEEDED(hr));

	// コマンドリストを生成する
	ComPtr<ID3D12GraphicsCommandList> commandList = nullptr;
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
		commandAllocator.Get(), nullptr,
		IID_PPV_ARGS(&commandList));
	assert(SUCCEEDED(hr));

	//***************************
	// 送信を実行
	//***************************
	// RunSenderTest();

	//***************************
	// 受信処理の初期化
	//***************************

	//// 1. ByteStream と mfReceiver を初期化した直後
	// std::shared_ptr<CustomByteStream> byteStream =
	// std::make_shared<CustomByteStream>(); MfH264Receiver mfReceiver;

	////mfReceiver.InitializeFromByteStream(device.Get(), commandQueue.Get(),
	/// byteStream.get());
	// for (int i = 0; i < 10; ++i) {

	//	if (mfReceiver.InitializeFromByteStream(device.Get(),
	// commandQueue.Get(), byteStream.get())) { 		break;
	//	}
	//	Sleep(200); // 200ms待ってリトライ
	//}

	//// 2. 受信スレッド開始（この直後に）
	// std::thread udpThread([byteStream]() {
	//	SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	//	sockaddr_in addr = {};
	//	addr.sin_family = AF_INET;
	//	addr.sin_port = htons(12345); // RTP/H264 ポート
	//	addr.sin_addr.s_addr = INADDR_ANY;

	//	bind(sock, (sockaddr*)&addr, sizeof(addr));

	//	while (true) {

	//		uint8_t buffer[2048];
	//		int recvLen = recv(sock, reinterpret_cast<char*>(buffer),
	// sizeof(buffer), 0); 		if (recvLen > 0) {
	// byteStream->PushData(buffer, recvLen);
	//		}
	//	}
	//	});
	// udpThread.detach(); // detach しておくとループと非同期に動作

	// MFStartup(MF_VERSION);

	// H264Encoder encoder;
	// if (!encoder.Initialize(1280, 720, 30)) {
	//	MessageBoxA(nullptr, "Encoder init failed", "Error", MB_OK);
	//	return 0;
	// }

	// std::vector<uint8_t> redFrame(1280 * 720 * 4, 255);
	// for (size_t i = 0; i < redFrame.size(); i += 4) {
	//	redFrame[i + 0] = 0;    // B
	//	redFrame[i + 1] = 0;    // G
	//	redFrame[i + 2] = 255;  // R
	//	redFrame[i + 3] = 255;  // A
	// }

	// std::ofstream ofs("test_output.h264", std::ios::binary);
	// std::vector<uint8_t> spspps;

	// for (int i = 0; i < 60; ++i) {
	//	std::vector<uint8_t> frameData;
	//	bool encoded = encoder.EncodeFrame(redFrame.data(),
	// static_cast<UINT>(redFrame.size()), frameData);

	//	if (!spspps.size()) {
	//		spspps = encoder.GetSpsPps();
	//		if (!spspps.empty()) {
	//			ofs.write(reinterpret_cast<const char*>(spspps.data()),
	// spspps.size()); 			OutputDebugStringA("SPS/PPS written\n");
	//		}
	//	}

	//	if (!frameData.empty()) {
	//		ofs.write(reinterpret_cast<const char*>(frameData.data()),
	// frameData.size()); 		OutputDebugStringA("Frame written\n");
	//	}
	//}
	// ofs.close();

	// MFShutdown();
	// MessageBoxA(nullptr, "H264 test complete", "Info", MB_OK);

	// CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	MFStartup(MF_VERSION);

	ColorConverter converter;
	H264Encoder encoder;

	UINT32 width = 1280;
	UINT32 height = 720;
	UINT32 fps = 30;
	UINT32 bitrate = 4000000; // // Winsock 初期化

	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		OutputDebugStringA("[ERROR] WSAStartup failed\n");
		return -1; // 適切に終了処理
	}

	//// 送信ソケットの準備（1回だけ）
	// SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	// sockaddr_in destAddr = {};
	// destAddr.sin_family = AF_INET;
	// destAddr.sin_port = htons(12345); // 宛先ポート
	// inet_pton(AF_INET, "127.0.0.1", &destAddr.sin_addr); // 宛先IP

	//// このsock, destAddrを使って各フレームを送る

	////// 2. 仮画像を先に生成しておく（実際に使うサイズのデータが存在）
	////BYTE* rgb32Frame = GenerateTestFrame(width, height);

	//// 1. 初期化
	// CameraCapture camera;
	// if (!camera.Initialize(width, height)) {
	//	OutputDebugStringA("[ERROR] Camera initialization failed\n");
	//	return -1;
	// }

	////// 初期化
	////if (!converter.Initialize(width, height)) {
	////	OutputDebugStringA("ColorConverter Init Failed\n");
	////	return 1;
	////}

	// if (!encoder.Initialize(width, height, bitrate, fps)) {
	//	OutputDebugStringA("H264Encoder Init Failed\n");
	//	return 1;
	// }

	/*ComPtr<IMFSample> inputSample;
	hr = CreateSampleFromRGB32(rgb32Frame, width, height, &inputSample);
	if (FAILED(hr)) {
			OutputDebugStringA("Sample failed\n");
			return 1;
	}*/

	//// 変換：RGB32 → NV12 (IMFSample)
	// ComPtr<IMFSample> nv12Sample;
	// if (!converter.Convert(inputSample.Get(), &nv12Sample)) {
	//	OutputDebugStringA("ColorConverter::Convert failed\n");
	//	return 1;
	// }

	/*std::vector<BYTE> h264Data;
	for (int i = 0; i < 5; ++i) {

			bool success = encoder.EncodeSample(nv12Sample.Get(), h264Data);
			if (!success) {
					OutputDebugStringA("EncodeSample failed\n");
					break;
			}
			char buf[128];
			sprintf_s(buf, "Frame %d: h264Data size = %zu\n", i, h264Data.size());
			OutputDebugStringA(buf);
	}*/

	// === (1) ファイルを開く ===
	FILE* file = nullptr;
	fopen_s(&file, "output.h264", "wb");
	if (!file) {
		OutputDebugStringA("[ERROR] 出力ファイルを開けませんでした\n");
		return 1;
	}



	// --- 共有リソースの定義 ---
	auto frameBufferMap =
		std::make_shared<std::map<uint16_t, std::vector<std::vector<uint8_t>>>>();
	auto frameBufferMutex = std::make_shared<std::mutex>();

	auto frameQueue = std::make_shared<std::queue<std::vector<uint8_t>>>();
	auto frameQueueMtx = std::make_shared<std::mutex>();
	auto frameQueueCv = std::make_shared<std::condition_variable>();

	//受信スレッド
	std::atomic<bool> stopRequested{ false };
	auto th = startTcpReceiverRgb32(
		frameQueue,
		frameQueueMtx,
		frameQueueCv,
		stopRequested,
		12345,
		1280,
		720
	);

	//// 終了時
	//std::thread rxRGB = startTcpReceiverRgb32(
	//	frameQueue, frameQueueMtx, frameQueueCv,
	//	rxStop, 12345, 1280, 720
	//);



	//// --- UDP受信・再構成スレッド ---
	// std::thread udpThread([frameBufferMap, frameBufferMutex, frameQueue,
	//                        frameQueueMtx, frameQueueCv]() {
	//   SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	//   if (sock == INVALID_SOCKET) {
	//     OutputDebugStringA("[ERROR] socket creation failed\n");
	//     return;
	//   }

	//  sockaddr_in addr = {};
	//  addr.sin_family = AF_INET;
	//  addr.sin_port = htons(12345);
	//  addr.sin_addr.s_addr = INADDR_ANY;

	//  if (bind(sock, (sockaddr *)&addr, sizeof(addr)) < 0) {
	//    OutputDebugStringA("[ERROR] bind() failed\n");
	//    closesocket(sock);
	//    return;
	//  }

	//  OutputDebugStringA("[INFO] UDP socket bound and listening...\n");

	//  static std::vector<uint8_t> spsData;
	//  static std::vector<uint8_t> ppsData;

	//  while (true) {
	//    uint8_t buffer[2048];
	//    int recvLen =
	//        recv(sock, reinterpret_cast<char *>(buffer), sizeof(buffer), 0);
	//    if (recvLen <= 6)
	//      continue;

	//    uint16_t frameID = (buffer[0] << 8) | buffer[1];
	//    uint8_t chunkID = buffer[2];
	//    uint8_t totalChunks = buffer[3];

	//    std::vector<uint8_t> payload(buffer + 6, buffer + recvLen);

	//    {
	//      std::lock_guard<std::mutex> lock(*frameBufferMutex);
	//      auto &chunks = (*frameBufferMap)[frameID];
	//      if (chunks.size() < totalChunks)
	//        chunks.resize(totalChunks);
	//      chunks[chunkID] = std::move(payload);

	//      // --- 再構成チェック ---
	//      bool complete = true;
	//      for (auto &c : chunks) {
	//        if (c.empty()) {
	//          complete = false;
	//          break;
	//        }
	//      }

	//      if (complete) {
	//        std::vector<uint8_t> fullFrame;
	//        for (auto &c : chunks)
	//          fullFrame.insert(fullFrame.end(), c.begin(), c.end());

	//        frameBufferMap->erase(frameID);
	//        OutputDebugStringA("[INFO] Reassembled full H.264 frame\n");

	//        // --- フレームをキューに追加 ---
	//        {
	//          std::lock_guard<std::mutex> ql(*frameQueueMtx);
	//          frameQueue->push(std::move(fullFrame));
	//          char buf[128];
	//          sprintf_s(buf, "[DEBUG] frameQueue size after push: %zu\n",
	//                    frameQueue->size());
	//          OutputDebugStringA(buf);
	//        }
	//        frameQueueCv->notify_one();
	//      }
	//    }
	//  }
	//});
	// udpThread.detach();

	// === 初回: IDR 先頭での初期化 & その場で Feed ===
// ※ ここは従来の「initFrame を取り出して push し直す」ブロックの置き換え

//// 軽量な Annex B 解析ラムダ
//	auto containsIDR = [&](const std::vector<uint8_t>& f) -> bool {
//		const size_t n = f.size();
//		auto isSC3 = [&](size_t k) { return k + 2 < n && f[k] == 0 && f[k + 1] == 0 && f[k + 2] == 1; };
//		auto isSC4 = [&](size_t k) { return k + 3 < n && f[k] == 0 && f[k + 1] == 0 && f[k + 2] == 0 && f[k + 3] == 1; };
//		auto nextSC = [&](size_t s)->size_t { for (size_t k = s; k + 3 < n; ++k) if (isSC3(k) || isSC4(k)) return k; return n; };
//
//		size_t p = nextSC(0);
//		while (p < n) {
//			size_t sc = isSC3(p) ? 3 : 4;
//			size_t hdr = p + sc;
//			size_t q = nextSC(hdr + 1);
//			if (hdr < n) {
//				uint8_t nal = f[hdr] & 0x1F;
//				if (nal == 5) return true; // IDR
//			}
//			p = q;
//		}
//		return false;
//		};
//
//	// SPS/PPS を保持しておく（Annex B: start code 付き丸ごと）
//	std::vector<uint8_t> spsData, ppsData;
//
//	// 1) まず 1 枚来るまで待つ（スプリアス対策で while）
//	{
//		std::unique_lock<std::mutex> lock(*frameQueueMtx);
//		frameQueueCv->wait(lock, [&] { return !frameQueue->empty(); });
//	}
//
//	// 2) IDR が先頭に来るまで、途中で見つけた SPS/PPS を記憶しつつ捨て進める
//	for (;;) {
//		std::unique_lock<std::mutex> lock(*frameQueueMtx);
//		frameQueueCv->wait(lock, [&] { return !frameQueue->empty(); });
//
//		// 先頭フレームを参照
//		const std::vector<uint8_t>& head = frameQueue->front();
//
//		// --- 先頭フレーム内の NAL を走査しつつ SPS/PPS を確保 ---
//		{
//			const size_t n = head.size();
//			auto isSC3 = [&](size_t k) { return k + 2 < n && head[k] == 0 && head[k + 1] == 0 && head[k + 2] == 1; };
//			auto isSC4 = [&](size_t k) { return k + 3 < n && head[k] == 0 && head[k + 1] == 0 && head[k + 2] == 0 && head[k + 3] == 1; };
//			auto nextSC = [&](size_t s)->size_t { for (size_t k = s; k + 3 < n; ++k) if (isSC3(k) || isSC4(k)) return k; return n; };
//
//			size_t p = nextSC(0);
//			while (p < n) {
//				const size_t sc = isSC3(p) ? 3 : 4;
//				const size_t hdr = p + sc;
//				const size_t q = nextSC(hdr + 1);
//				if (hdr < n) {
//					uint8_t nalType = head[hdr] & 0x1F;
//					if (nalType == 7) { // SPS
//						spsData.assign(head.begin() + p, (q <= n ? head.begin() + q : head.end()));
//						OutputDebugStringA("[INIT] SPS captured\n");
//					}
//					else if (nalType == 8) { // PPS
//						ppsData.assign(head.begin() + p, (q <= n ? head.begin() + q : head.end()));
//						OutputDebugStringA("[INIT] PPS captured\n");
//					}
//				}
//				p = q;
//			}
//		}
//
//		if (containsIDR(head)) {
//			// --- IDR が先頭に来た！この1枚で初期化＆即 Feed する ---
//			OutputDebugStringA("[INFO] IDR at queue head. Proceeding to Initialize\n");
//
//			// フレームを取り出す
//			std::vector<uint8_t> idr = std::move(frameQueue->front());
//			frameQueue->pop();
//			lock.unlock();
//
//			// 必要なら SPS/PPS を前置
//			std::vector<uint8_t> idrWithHeader;
//			if (!spsData.empty() && !ppsData.empty()) {
//				idrWithHeader.reserve(spsData.size() + ppsData.size() + idr.size());
//				idrWithHeader.insert(idrWithHeader.end(), spsData.begin(), spsData.end());
//				idrWithHeader.insert(idrWithHeader.end(), ppsData.begin(), ppsData.end());
//				idrWithHeader.insert(idrWithHeader.end(), idr.begin(), idr.end());
//			}
//			else {
//				// 送出側が IDR 内に SPS/PPS を同梱しているなら無くても動くが、
//				// 安定のためあれば前置推奨。ここでは無ければそのまま進む。
//				idrWithHeader = std::move(idr);
//				OutputDebugStringA("[WARN] SPS/PPS not captured before IDR; feeding as-is\n");
//			}
//
//			// ★ デコーダ初期化（解像度が既知なら固定値、SPS から読むなら適宜置換）
//			MfH264Receiver mfReceiver;
//			if (!mfReceiver.InitializeDecoder(1280, 720)) {
//				OutputDebugStringA("[ERROR] Decoder init failed\n");
//				// 初期化に失敗したら、この IDR を捨てて次の IDR を待つ手もある
//				// ここでは return で中断しておく
//				// （スレッド外でエラー処理をしたいならフラグにする等）
//				return 1;
//			}
//
//			// 初回 IDR を即投入（100ns 単位 PTS。とりあえず 0 始まり/60fps 前提）
//			static LONGLONG s_pts100ns = 0;
//			const LONGLONG kDur = 166666; // ≒1/60s
//			s_pts100ns += kDur;
//
//			HRESULT hr = mfReceiver.FeedAccessUnit(
//				idrWithHeader.data(),
//				static_cast<DWORD>(idrWithHeader.size()),
//				s_pts100ns,
//				TRUE // isIDR
//			);
//			if (FAILED(hr)) {
//				OutputDebugStringA("[ERROR] FeedAccessUnit (init IDR) failed\n");
//				return 1;
//			}
//
//			OutputDebugStringA("[INFO] Decoder initialized and first IDR fed. Continue normal feed loop.\n");
//			break; // 初期化完了。以降は通常の受信→Feed へ。
//		}
//
//		// 先頭が IDR でなければ捨てて回す（SPS/PPS は既に抜き取って保持済み）
//		frameQueue->pop();
//		// ループ継続 → 次のフレーム到着を待つ
//	}

// // 2) MJPEGレシーバ作成（外部キューを紐づけ）
//MfH264Receiver decoder;
//if (FAILED(decoder.InitializeDecoder(width,height))) {
//	OutputDebugStringA("[INIT] decoder.InitializeDecoder failed\n");
//
//	// 必要なら stop/Join して終了
//}

	// 以降：通常ループ側で frameQueue から取り出して FeedAccessUnit していく
	//   - IDR 以降は Non-IDR でも順次 Feed してOK
	//   - 必要なら一定間隔で SPS/PPS を前置する運用にしてもよい


	//{
	//	std::unique_lock<std::mutex> lock(*frameQueueMtx);
	//	// 条件変数を使って、キューが空でなくなるまで待機
	//	frameQueueCv->wait(lock, [&] { return !frameQueue->empty(); });
	//}
	// OutputDebugStringA("[INFO] First fullFrame arrived, proceeding to
	// Initialize\n");
	//
	//
	//	// 初期化（SPS後なので失敗しない）
	//	MfH264Receiver mfReceiver;
	//
	//	if (!mfReceiver.InitializeFromDecoderOnly(device.Get(),
	// commandQueue.Get())) { 		OutputDebugStringA("Initialization
	// failed (even after SPS detection)\n"); 		encoder.Shutdown();
	// return 1;
	//	}

	// スワップチェーンを生成する
	graphics::SwapChain swapChain;
	if (!swapChain.Create(dev, hwnd, /*width*/ 1280, /*height*/ 720, /*buffers*/ 2)) {
		return -1;
	}

	core::CommandListPool clPool;
	clPool.Initialize(dev, /*frameCount=*/swapChain.BufferCount());



	//ComPtr<IDXGISwapChain4> swapChain = nullptr;
	//DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	//swapChainDesc.Width = wd.width;
	//swapChainDesc.Height = wd.height;
	//swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	//swapChainDesc.SampleDesc.Count = 1;
	//swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	//swapChainDesc.BufferCount = 2;

	//// モニターに移したら,中身を確認
	//swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	//// コマンドキュー,ウィンドウハンドル,設定を渡して生成する
	//hr = dxgiFactory->CreateSwapChainForHwnd(
	//	commandQueue.Get(), hwnd, &swapChainDesc, nullptr, nullptr,
	//	reinterpret_cast<IDXGISwapChain1**>(swapChain.GetAddressOf()));

	//assert(SUCCEEDED(hr));
	////デスクリープの生成
	// ID3D12DescriptorHeap* rtvDescriptorHeap = nullptr;
	// D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc{};
	// rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	// rtvDescriptorHeapDesc.NumDescriptors = 2;
	// hr = device->CreateDescriptorHeap(&rtvDescriptorHeapDesc,
	// IID_PPV_ARGS(&rtvDescriptorHeap));
	//  RTV用のヒープでディスクリプタの数は2。RTVはShader内で触るものではないので、ShaderVisibleはfalse

	ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap =
		CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);

	// SRV用のヒープでディスクリプタの数は128。SRVはShader内で触るものなので、ShaderVisibleはtrue
	ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap = CreateDescriptorHeap(
		device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);

	ComPtr<ID3D12Resource> depthStencilResource =
		CreateDepthStencilTextureResource(device, wd.width, wd.height);

	// DSV用のヒープでディスクリプタの数は1。DSVはShader内で触るものではないので、ShaderVisibleはfalse
	ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap =
		CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

	// DSVの設定
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};

	// Format。基本的にはResourceに合わせる
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// 2dTexture
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

	// Heap上の先頭にDSVをつくる
	device->CreateDepthStencilView(
		depthStencilResource.Get(), &dsvDesc,
		dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	//**************************
	// DescriptorSizeを取得
	//**************************
	uint32_t descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	uint32_t descriptorSizeRTV =
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	uint32_t descriptorSizeDSV =
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	// ディスクリプタひーおうが作れなっかたので起動できない
	assert(SUCCEEDED(hr));

	//ComPtr<ID3D12Resource> swapChainResources[2] = { nullptr };
	//hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));

	//assert(SUCCEEDED(hr));
	//hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
	//assert(SUCCEEDED(hr));

	//D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	//rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	//rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	//// ディスクリプタの先頭を取得する
	//D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle =
	//	GetCPUDescriptorHandle(rtvDescriptorHeap, descriptorSizeRTV, 0);

	//// RTVを2つ作るのでディスクリプタを2つ用意
	//D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];

	//rtvHandles[0] = rtvStartHandle;
	//device->CreateRenderTargetView(swapChainResources[0].Get(), &rtvDesc,
	//	rtvHandles[0]);

	//rtvHandles[1].ptr =
	//	rtvHandles[0].ptr +
	//	device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	//device->CreateRenderTargetView(swapChainResources[1].Get(), &rtvDesc,
	//	rtvHandles[1]);

	//// バックバッファ番号
	//UINT bb = swapChain.CurrentIndex();

	//// そのフレーム用のRTVとBackBuffer
	//D3D12_CPU_DESCRIPTOR_HANDLE rtv = swapChain.RTV(bb);
	//ID3D12Resource* backBuffer = swapChain.BackBuffer(bb);

	constexpr DXGI_FORMAT kRtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	uint64_t fenceValue = 0;

	// FenceのSignalを持つためのイベントを作成する
	HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent != nullptr);

	// dxcCompilerを初期化
	IDxcUtils* dxcUtils = nullptr;
	IDxcCompiler3* dxcCompiler = nullptr;
	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
	assert(SUCCEEDED(hr));

	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
	assert(SUCCEEDED(hr));

	// 現時点でincludeはしないが、includeに対応するための設定を行っておく
	IDxcIncludeHandler* includeHandler = nullptr;
	hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);

	assert(SUCCEEDED(hr));

	//**************************
	// RootSignature
	//**************************

	// RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};

	// Samplerの設定
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};

	staticSamplers[0].Filter =
		D3D12_FILTER_MIN_MAG_MIP_LINEAR; // バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // U軸：繰り返し
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // V軸：繰り返し
	staticSamplers[0].AddressW =
		D3D12_TEXTURE_ADDRESS_MODE_WRAP; // W軸（3D用）：繰り返し
	staticSamplers[0].ComparisonFunc =
		D3D12_COMPARISON_FUNC_NEVER; // 比較しない（Shadow Map等で使う）
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX; // 使えるだけMipmap使う
	staticSamplers[0].ShaderRegister = 0;         // register(s0)
	staticSamplers[0].ShaderVisibility =
		D3D12_SHADER_VISIBILITY_PIXEL; // PixelShader用

	// RootSignature にバインド
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	descriptionRootSignature.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};

	descriptorRange[0].BaseShaderRegister = 0; // register(t0) から始まる
	descriptorRange[0].NumDescriptors = 1;     // 数は1つ
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRV用
	descriptorRange[0].OffsetInDescriptorsFromTableStart = 0; // t0用（先頭）

	// 受信画像用だけの descriptorRange を別に定義
	D3D12_DESCRIPTOR_RANGE receivedRange = {};
	receivedRange.BaseShaderRegister = 4; // t4
	receivedRange.NumDescriptors = 1;
	receivedRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	receivedRange.OffsetInDescriptorsFromTableStart = 0;

	// === motionMaskTex 用 DescriptorRange（t2）を追加 ===
	D3D12_DESCRIPTOR_RANGE motionMaskRange = {};
	motionMaskRange.BaseShaderRegister = 2; // register(t2)
	motionMaskRange.NumDescriptors = 1;
	motionMaskRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	motionMaskRange.OffsetInDescriptorsFromTableStart = 0;

	// RootParameter作成。接続数をできるので配列。今回は根根1つだけなので長さ1の配列
	D3D12_ROOT_PARAMETER rootParameters[6] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[0].ShaderVisibility =
		D3D12_SHADER_VISIBILITY_PIXEL;               // PixelShaderで使う
	rootParameters[0].Descriptor.ShaderRegister = 0; // レジスタ番号0にバインド

	// VertexShader用のCBV（Transform）
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[1].ShaderVisibility =
		D3D12_SHADER_VISIBILITY_VERTEX; // VertexShaderで使う
	rootParameters[1].Descriptor.ShaderRegister = 0;

	// DescriptorTable
	rootParameters[2].ParameterType =
		D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う
	rootParameters[2].ShaderVisibility =
		D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges =
		descriptorRange; // Tableの中身の配列を指定
	rootParameters[2].DescriptorTable.NumDescriptorRanges =
		_countof(descriptorRange); // Rangeの数（今回は1つ）

	// 平行光源用
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[3].ShaderVisibility =
		D3D12_SHADER_VISIBILITY_PIXEL;               // PixelShaderで使う
	rootParameters[3].Descriptor.ShaderRegister = 1; // レジスタ番号 b1 を使う

	// rootParameters[4] : 受信用テクスチャの SRV テーブル
	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[4].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[4].DescriptorTable.pDescriptorRanges = &receivedRange;

	// rootParameters[4] : 受信用テクスチャの SRV テーブル
	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[5].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[5].DescriptorTable.pDescriptorRanges = &motionMaskRange;

	// ルートパラメータ配列へのポインタ
	descriptionRootSignature.pParameters = rootParameters;

	// 配列の長さ
	descriptionRootSignature.NumParameters = _countof(rootParameters);

	// WVP用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	ComPtr<ID3D12Resource> wvpResource =
		CreateBufferResource(device, sizeof(Matrix4x4));

	// データを書き込む
	Matrix4x4* wvpData = nullptr;

	// 書き込むためのアドレスを取得
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));

	// 単位行列を書きこんでおく
	*wvpData = MakeIdentity4x4();

	// シリアライズしてバイナリにする
	ID3DBlob* signatureBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob,
		&errorBlob);
	if (FAILED(hr)) {
		Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	// バイナリを元に生成
	ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature));

	if (FAILED(hr)) {
		Log("RootSignature creation failed.");

		assert(false);
	}

	assert(SUCCEEDED(hr));

	//***************************
	// Compute用 RootSignature
	//***************************

	D3D12_DESCRIPTOR_RANGE srvRange = {};
	srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvRange.NumDescriptors = 2;
	srvRange.BaseShaderRegister = 4; // ✅ t4から始まる ← HLSLに合わせる
	srvRange.RegisterSpace = 0;
	srvRange.OffsetInDescriptorsFromTableStart = 0;

	D3D12_DESCRIPTOR_RANGE uavRange = {};
	uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRange.NumDescriptors = 1;
	uavRange.BaseShaderRegister = 0; // u0
	uavRange.RegisterSpace = 0;
	uavRange.OffsetInDescriptorsFromTableStart = 0;

	D3D12_ROOT_PARAMETER rootParams[2] = {};

	// --- SRV (t4, t5)
	rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[0].DescriptorTable.pDescriptorRanges = &srvRange;
	rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// --- UAV (u0)
	rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[1].DescriptorTable.pDescriptorRanges = &uavRange;
	rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// --- RootSignature構築
	D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
	rootSigDesc.NumParameters = _countof(rootParams);
	rootSigDesc.pParameters = rootParams;
	rootSigDesc.NumStaticSamplers = 0;
	rootSigDesc.pStaticSamplers = nullptr;
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	signatureBlob = nullptr;
	errorBlob = nullptr;
	hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		&signatureBlob, &errorBlob);

	if (FAILED(hr)) {
		if (errorBlob) {
			OutputDebugStringA((char*)errorBlob->GetBufferPointer());
		}
		assert(false);
	}
	ComPtr<ID3D12RootSignature> computeRootSignature;
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&computeRootSignature));
	assert(SUCCEEDED(hr));

	// D3D12_DESCRIPTOR_RANGE srvRange = {};
	// srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	// srvRange.NumDescriptors = 2;     // t0, t1
	// srvRange.BaseShaderRegister = 0; // t0から始まる
	// srvRange.RegisterSpace = 0;
	// srvRange.OffsetInDescriptorsFromTableStart = 0;

	// D3D12_DESCRIPTOR_RANGE uavRange = {};
	// uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	// uavRange.NumDescriptors = 1; // u0
	// uavRange.BaseShaderRegister = 0;
	// uavRange.RegisterSpace = 0;
	// uavRange.OffsetInDescriptorsFromTableStart = 0;

	//// RootParameter 配列
	// D3D12_ROOT_PARAMETER rootParams[3] = {};

	//// --- SRVテーブル（今のフレーム + 前のフレーム）
	// rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	// rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
	// rootParams[0].DescriptorTable.pDescriptorRanges = &srvRange;
	// rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	//// --- UAVテーブル（動体マスク出力）
	// rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	// rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
	// rootParams[1].DescriptorTable.pDescriptorRanges = &uavRange;
	// rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	//// --- CBV（しきい値 threshold）
	// rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	// rootParams[2].Descriptor.ShaderRegister = 0; // register(b0)
	// rootParams[2].Descriptor.RegisterSpace = 0;
	// rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	//// RootSignature 記述
	// D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
	// rootSigDesc.NumParameters = _countof(rootParams);
	// rootSigDesc.pParameters = rootParams;
	// rootSigDesc.NumStaticSamplers = 0;
	// rootSigDesc.pStaticSamplers = nullptr;
	// rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	//// シリアライズと作成
	// signatureBlob = nullptr;
	// errorBlob = nullptr;
	// hr = D3D12SerializeRootSignature(&rootSigDesc,
	// D3D_ROOT_SIGNATURE_VERSION_1,
	//                                  &signatureBlob, &errorBlob);

	// if (FAILED(hr)) {
	//   if (errorBlob) {
	//     OutputDebugStringA((char *)errorBlob->GetBufferPointer());
	//   }
	//   assert(false);
	// }

	// ComPtr<ID3D12RootSignature> computeRootSignature;
	// hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
	//                                  signatureBlob->GetBufferSize(),
	//                                  IID_PPV_ARGS(&computeRootSignature));

	// assert(SUCCEEDED(hr));

	// --- ① リソース作成（サイズはfloat 1つ分）
	ComPtr<ID3D12Resource> thresholdCBV =
		CreateBufferResource(device.Get(), sizeof(float));

	// --- ② 書き込み用ポインタ取得
	float* thresholdData = nullptr;
	thresholdCBV->Map(0, nullptr, reinterpret_cast<void**>(&thresholdData));

	// --- ③ 初期値（例えば0.1fなど）
	*thresholdData = 0.1f;

	// ✳️ 注意：今後、しきい値をリアルタイムで変更したい場合はこの thresholdData
	// に対して書き換えるだけでOK

	//**************************
	// InputLayout
	//**************************

	// InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	// TEXCOORD
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	// NORMAL
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	//**************************
	// BlendStateの設定
	//**************************

	// BlendStateの設定
	D3D12_BLEND_DESC blendDesc{};

	// すべての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask =
		D3D12_COLOR_WRITE_ENABLE_ALL;
	/* ✨ 追加：マスク用ブレンド有効化
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;*/

	//**************************
	// RasterizerStateの設定
	//**************************

	// RasterizerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};

	// 裏面（時計回り）を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;

	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	//**************************
	// Shaderをコンパイルする
	//**************************

	// Shaderをコンパイルする
	IDxcBlob* vertexShaderBlob = CompileShader(
		L"Object3D.VS.hlsl", L"vs_6_0", dxcUtils, dxcCompiler, includeHandler);
	assert(vertexShaderBlob != nullptr);

	IDxcBlob* pixelShaderBlob = CompileShader(
		L"Object3D.PS.hlsl", L"ps_6_0", dxcUtils, dxcCompiler, includeHandler);
	assert(pixelShaderBlob != nullptr);

	IDxcBlob* csBlob = CompileShader(L"MotionDetect.CS.hlsl", L"cs_6_0", dxcUtils,
		dxcCompiler, includeHandler);
	assert(csBlob != nullptr);

	// DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};

	// Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;

	// 書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

	// 比較関数はLessEqual。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	//**************************
	// PSOを生成する
	//**************************
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};

	// RootSignature
	graphicsPipelineStateDesc.pRootSignature = rootSignature.Get();

	// InputLayout
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;

	// VertexShader
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
									vertexShaderBlob->GetBufferSize() };

	// PixelShader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
									pixelShaderBlob->GetBufferSize() };

	// BlendState
	graphicsPipelineStateDesc.BlendState = blendDesc;

	// RasterizerState
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;

	// DepthStencilの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// 書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

	// 利用するポリゴン（形状）のタイプ：三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	// どのように画面に色を打ち込むかの設定（気にしなくて良い）
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// 実際に生成
	ComPtr<ID3D12PipelineState> graphicsPipelineState = nullptr;
	hr = device->CreateGraphicsPipelineState(
		&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));

	assert(SUCCEEDED(hr));

	// Compute Shader を使う PSO
	D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc{};
	computePsoDesc.pRootSignature =
		computeRootSignature.Get(); // RootSignatureは別途定義
	computePsoDesc.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };

	ComPtr<ID3D12PipelineState> computePSO;
	hr = device->CreateComputePipelineState(&computePsoDesc,
		IID_PPV_ARGS(&computePSO));
	assert(SUCCEEDED(hr));

	//**************************
	//alphaBlendPSOを生成する
	//**************************
	auto MakeOpaqueBlend = []() {
		D3D12_BLEND_DESC d{};
		d.AlphaToCoverageEnable = FALSE;
		d.IndependentBlendEnable = FALSE;
		auto& rt = d.RenderTarget[0];
		rt.BlendEnable = FALSE; // 不透明（ブレンド無効）
		rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		return d;
		};

	auto MakeAlphaBlend = []() {
		D3D12_BLEND_DESC d{};
		d.AlphaToCoverageEnable = FALSE;
		d.IndependentBlendEnable = FALSE;
		auto& rt = d.RenderTarget[0];
		rt.BlendEnable = TRUE;                           // アルファブレンド
		rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		rt.BlendOp = D3D12_BLEND_OP_ADD;
		rt.SrcBlendAlpha = D3D12_BLEND_ONE;
		rt.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		return d;
		};

	// 既存の設定をベースに PSO 設定を組み立て（共通部分）
	D3D12_GRAPHICS_PIPELINE_STATE_DESC baseDesc{};
	baseDesc.pRootSignature = rootSignature.Get();       // 既存を流用
	baseDesc.InputLayout = inputLayoutDesc;         // 既存を流用
	baseDesc.VS = { vertexShaderBlob->GetBufferPointer(),
									vertexShaderBlob->GetBufferSize() };;                  // 既存を流用
	baseDesc.PS = { pixelShaderBlob->GetBufferPointer(),
									pixelShaderBlob->GetBufferSize() };                  // 既存を流用
	baseDesc.RasterizerState = rasterizerDesc;      // 既存を流用
	baseDesc.DepthStencilState = depthStencilDesc;    // 既存を流用（必要に応じて透過用でWriteMask変更）
	baseDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	baseDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	baseDesc.NumRenderTargets = 1;
	baseDesc.RTVFormats[0] = DXGI_FORMAT_D24_UNORM_S8_UINT;        // 既存を流用
	baseDesc.DSVFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;           // 既存を流用
	baseDesc.SampleDesc.Count = 1;

	// --- 不透明 PSO ---
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueDesc = baseDesc;
	opaqueDesc.BlendState = MakeOpaqueBlend();

	ComPtr<ID3D12PipelineState> psoOpaque;
	{
		HRESULT hr = device->CreateGraphicsPipelineState(&opaqueDesc, IID_PPV_ARGS(&psoOpaque));
		// （任意）if(FAILED(hr)) { /* ログ */ }
	}

	// --- アルファブレンド PSO ---
	// 透過は深度書き込みを切ると破綻が減る：必要ならここで上書き
	D3D12_GRAPHICS_PIPELINE_STATE_DESC alphaDesc = baseDesc;
	alphaDesc.BlendState = MakeAlphaBlend();
	alphaDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // 任意

	ComPtr<ID3D12PipelineState> psoAlpha;
	{
		HRESULT hr = device->CreateGraphicsPipelineState(&alphaDesc, IID_PPV_ARGS(&psoAlpha));
		// （任意）if(FAILED(hr)) { /* ログ */ }
	}

	//**************************
	// VertexResourceを生成する
	//**************************

	// 頂点リソース用のヒープの設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; // UploadHeapを使う

	// 頂点リソースの設定
	D3D12_RESOURCE_DESC vertexResourceDesc{};

	// バッファリソース。テクスチャの場合はまた別の設定をする
	vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexResourceDesc.Width =
		sizeof(VertexData) * 3; // リソースのサイズ。今回はVector4を3頂点分

	// バッファの場合はこれらはすべて決まり
	vertexResourceDesc.Height = 1;
	vertexResourceDesc.DepthOrArraySize = 1;
	vertexResourceDesc.MipLevels = 1;
	vertexResourceDesc.SampleDesc.Count = 1;
	vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// 実際に頂点リソースを作る
	ComPtr<ID3D12Resource> vertexResource =
		CreateBufferResource(device, sizeof(VertexData) * 6);
	hr = device->CreateCommittedResource(
		&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &vertexResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&vertexResource));
	assert(SUCCEEDED(hr));

	ComPtr<ID3D12Resource> indexResourceSprite =
		CreateBufferResource(device, sizeof(uint32_t) * 6);

	// —————— ① 分割数を定義 ——————
	const UINT stackCount = 16; // 緯度方向の分割数
	const UINT sliceCount = 32; // 経度方向の分割数

	// Sprite用の頂点リソースを作る
	ComPtr<ID3D12Resource> vertexResourceSprite =
		CreateBufferResource(device, sizeof(VertexData) * 6);

	DirectionalLight directionalLightData;

	// デフォルト値としてとりあえず以下のようにしておく
	directionalLightData.color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白色光

	Vector3 dir = { 0.0f, -1.0f, 0.0f };

	// 方向を正規化する
	float length = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
	dir.x /= length;
	dir.y /= length;
	dir.z /= length;
	directionalLightData.direction = dir;

	directionalLightData.intensity = 1.0f;
	ComPtr<ID3D12Resource> directionalLightResource =
		CreateBufferResource(device, sizeof(DirectionalLight));
	DirectionalLight* mappedLight = nullptr;
	directionalLightResource->Map(0, nullptr,
		reinterpret_cast<void**>(&mappedLight));

	// まずVertexShaderで利用するtransformationMatrix用のResourceを作る
	// Sprite用のTransformationMatrix用のリソースを作る。Matrix4x4
	// 1つ分のサイズを用意する
	ComPtr<ID3D12Resource> transformationMatrixResourceSprite =
		CreateBufferResource(device, sizeof(Matrix4x4));

	// データを書き込む
	Matrix4x4* transformationMatrixDataSprite = nullptr;

	// 書き込むためのアドレスを取得
	transformationMatrixResourceSprite->Map(
		0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite)

	);

	// 単位行列を書きこんでおく
	*transformationMatrixDataSprite = MakeIdentity4x4();

	// CPUで動かす用のTransformを作る
	Transform transformSprite{
		{1.0f, 1.0f, 1.0f}, // scale
		{0.0f, 0.0f, 0.0f}, // rotation
		{0.0f, 0.0f, 0.0f}  // translation
	};

	//**************************
	// VertexBufferView（VBV）を作成する
	//**************************
	//// 頂点バッファビューを作成する
	// D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};

	//// リソースの先頭のアドレスから使う
	// vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();

	//// 使用するリソースのサイズは頂点3つ分のサイズ
	// vertexBufferView.SizeInBytes = sizeof(VertexData) * 6;

	//// 1頂点あたりのサイズ
	// vertexBufferView.StrideInBytes = sizeof(VertexData);

	//// —————— ④ VBV を設定 ——————
	// D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSphere{};
	// vertexBufferViewSphere.BufferLocation =
	// vertexResourceSphere->GetGPUVirtualAddress();
	// vertexBufferViewSphere.SizeInBytes = sizeof(VertexData) *
	// sphereVertexCount; vertexBufferViewSphere.StrideInBytes =
	// sizeof(VertexData);

	// 頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};

	// リソースの先頭のアドレスから使う
	vertexBufferViewSprite.BufferLocation =
		vertexResourceSprite->GetGPUVirtualAddress();

	// 使用するリソースのサイズは頂点6つ分のサイズ
	vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 6;

	// 1頂点あたりのサイズ
	vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

	// IndexBufferViewを作成する
	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};

	// リソースの先頭のアドレスから使う
	indexBufferViewSprite.BufferLocation =
		indexResourceSprite->GetGPUVirtualAddress();

	// 使用するリソースのサイズはインデックス6つ分のサイズ
	indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;

	// インデックスはuint32として使う
	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;

	//**************************
	// 頂点リソースにデータを書き込む
	//**************************
	// 頂点リソースにデータを書き込む
	Vector4* vertexData = nullptr;

	//// 書き込むためのアドレスを取得
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

	// インデックスリソースにデータを書き込む
	uint32_t* indexDataSprite = nullptr;
	indexResourceSprite->Map(0, nullptr,
		reinterpret_cast<void**>(&indexDataSprite));
	indexDataSprite[0] = 0;
	indexDataSprite[1] = 1;
	indexDataSprite[2] = 2;
	indexDataSprite[3] = 1;
	indexDataSprite[4] = 3;
	indexDataSprite[5] = 2;

	// 今回は右のような矩形（三角形2枚）として、
	// ローカル頂点を構成する。黒字が座標で、橙字がTexcoordである
	VertexData* vertexDataSprite = nullptr;

	vertexResourceSprite->Map(0, nullptr,
		reinterpret_cast<void**>(&vertexDataSprite));
	// 1枚目の三角形
	vertexDataSprite[0].position = { 0.0f, 256.0f, 0.0f, 1.0f }; // 左下
	vertexDataSprite[0].texcoord = { 0.0f, 1.0f };
	vertexDataSprite[1].position = { 0.0f, 0.0f, 0.0f, 1.0f }; // 左上
	vertexDataSprite[1].texcoord = { 0.0f, 0.0f };
	vertexDataSprite[2].position = { 360.0f, 256.0f, 0.0f, 1.0f }; // 右下
	vertexDataSprite[2].texcoord = { 1.0f, 1.0f };

	// 2枚目の三角形
	vertexDataSprite[3].position = { 0.0f, 0.0f, 0.0f, 1.0f }; // 左上
	vertexDataSprite[3].texcoord = { 0.0f, 0.0f };
	vertexDataSprite[4].position = { 360.0f, 0.0f, 0.0f, 1.0f }; // 右上
	vertexDataSprite[4].texcoord = { 1.0f, 0.0f };
	vertexDataSprite[5].position = { 360.0f, 256.0f, 0.0f, 1.0f }; // 右下
	vertexDataSprite[5].texcoord = { 1.0f, 1.0f };

	// マテリアル用のリソースを作る。今回は color1つ分のサイズを用意する
	ComPtr<ID3D12Resource> materialResource =
		CreateBufferResource(device, sizeof(Material));

	// マテリアルにデータを書き込む
	Material* materialData = nullptr;

	// 書き込むためのアドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));

	// 今回は赤を書き込んでみる
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

	materialData->enableLighting = true;

	// UVTransform行列を単位行列で初期化
	materialData->uvTransform = MakeIdentity4x4();

	Material* materialDataSprite;

	// Sprite用のマテリアルリソースを作る。
	ComPtr<ID3D12Resource> materialResourceSprite =
		CreateBufferResource(device, sizeof(Material));

	// 書き込むためのアドレスを取得
	materialResourceSprite->Map(0, nullptr,
		reinterpret_cast<void**>(&materialDataSprite));
	// 今回は赤を書き込んでみる
	materialDataSprite->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

	materialDataSprite->enableLighting = false;

	// UVTransform行列を単位行列で初期化
	materialDataSprite->uvTransform = MakeIdentity4x4();

	//**************************
	// ビューポート（画面への変換領域）設定
	//**************************
	D3D12_VIEWPORT viewport{};
	// クライアント領域のサイズと一致させて画面全体に表示
	viewport.Width = static_cast<float>(wd.width);
	;
	viewport.Height = static_cast<float>(wd.height);
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	//**************************
	// シザー矩形（描画可能範囲の切り抜き）
	//**************************
	D3D12_RECT scissorRect{};
	// 基本的にビューポートと同じサイズにして描画領域を制限しないようにする
	scissorRect.left = 0;
	scissorRect.right = wd.width;
	scissorRect.top = 0;
	scissorRect.bottom = wd.height;

	// Transform変数を作る
	Transform transform{
		{1.0f, 1.0f, 1.0f}, // scale（等倍）
		{0.0f, 0.0f, 0.0f}, // rotate（回転なし）
		{0.0f, 0.0f, 0.0f}  // translate（原点）
	};

	// カメラTransform（Z+ 方向を向いた位置に配置）
	Transform cameraTransform{
		{1.0f, 1.0f, 1.0f}, // scale
		{0.0f, 0.0f, 0.0f}, // rotate
		{0.0f, 0.0f, -5.0f} // translate
	};

	// 射影行列
	Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(
		0.45f, float(wd.width) / float(wd.height), 0.1f, 100.0f);

	// UVTransform用の変数
	Transform uvTransformSprite{
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f},
	};

	// 例：テクスチャを貼るための簡単な四角形の頂点
	struct Vertex {
		Vector3 pos;
		Vector2 uv;
	};

	// 画面全体に貼るなら以下のようなデータを使う
	Vertex vertices[] = {
		{{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
		{{-1.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
		{{1.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
		{{1.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
	};

	// インデックス
	uint16_t indices[] = {
		0, 1, 2, 2, 1, 3,
	};

	// ImGuiの初期化。詳細はさして重要ではないので解説は省略する。
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX12_Init(device.Get(), static_cast<int>(swapChain.BufferCount()), kRtvFormat,
		srvDescriptorHeap.Get(),
		srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	//**************************
	// Texctureを読んで転送する
	//**************************
	DirectX::ScratchImage mipImages = LoadTexture("resources/uvChecker.png");
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	ComPtr<ID3D12Resource> textureResource =
		CreateTextureResource(device, metadata);
	UploadTextureData(textureResource, mipImages);

	// 2枚目のTextureを読んで転送
	DirectX::ScratchImage mipImages2 = LoadTexture("resources/monsterBall.png");
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	ComPtr<ID3D12Resource> textureResource2 =
		CreateTextureResource(device, metadata2);
	UploadTextureData(textureResource2, mipImages2);

	// metaDataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	// SRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU =
		srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU =
		srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

	// 先頭はImGuiが使っているのでその次を使う
	textureSrvHandleCPU.ptr += device->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	textureSrvHandleGPU.ptr += device->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// SRVの生成
	device->CreateShaderResourceView(textureResource.Get(), &srvDesc,
		textureSrvHandleCPU);

	// metaDataを基にSRVの設定(2回目)
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format = metadata2.format;
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

	// SRVを作成するDescriptorHeapの場所を決める(2回目)
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 =
		GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 =
		GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);

	// SRVの生成(2回目)
	device->CreateShaderResourceView(textureResource2.Get(), &srvDesc2,
		textureSrvHandleCPU2);

	SphereMeshData sphere{};

	//// コマンドアロケータとコマンドリストをReset
	//commandAllocator->Reset();
	//commandList->Reset(commandAllocator.Get(), graphicsPipelineState.Get());

	bool useMonsterBall = true;

	//****************************
	// MotionDetect用のリソースを作成
	//****************************
	const UINT texWidth = 1280;
	const UINT texHeight = 720;
	DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
	ComPtr<ID3D12Resource> texture =
		CreateTextureResourceResolution(device, texWidth, texHeight, format);


	// --- thresholdCBV を作成 ---
	thresholdCBV = CreateBufferResource(device.Get(), sizeof(float));

	// === 1. ディスクリプタサイズと開始位置を取得 ===
	UINT descriptorSize = device->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// 元のデコード用 NV12 テクスチャ
	// ID3D12Resource* nv12Tex = texture.Get();  //
	// CreateTextureResourceResolution(..., NV12)
	// D3D12 の空テクスチャを先に作っておく
	// ComPtr<ID3D12Resource> nv12Tex12 = CreateTextureResourceResolution(device,
	// 1280, 720, DXGI_FORMAT_NV12);

	D3D12_CPU_DESCRIPTOR_HANDLE yHandle = GetCPUDescriptorHandle(
		srvDescriptorHeap, descriptorSize, 4); // Y plane: t4

	D3D12_CPU_DESCRIPTOR_HANDLE uvHandle = GetCPUDescriptorHandle(
		srvDescriptorHeap, descriptorSize, 5); // UV plane: t5



	// t7スロットに描画用SRVを作る例（他と被らないスロット）
	D3D12_CPU_DESCRIPTOR_HANDLE drawSRVHandleCPU =
		GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSize, 7);
	D3D12_GPU_DESCRIPTOR_HANDLE drawSRVHandleGPU =
		GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSize, 7);


	// === 4. motionMaskTex（u0）UAV を作成 ===
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;
	uavDesc.Texture2D.PlaneSlice = 0;


	// === 5. GPU 側ハンドルの取得（Dispatch用） ===
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandleStart =
		GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSize, 0); // t0 / t1
	D3D12_GPU_DESCRIPTOR_HANDLE uavHandle =
		GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSize, 2); // u0

	// GPU 側ハンドルも確保（あとで描画パスに使う）
	D3D12_GPU_DESCRIPTOR_HANDLE motionMaskSRVHandle =
		GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSize, 3);

	// --- 書き込み用ポインタ取得
	thresholdData = nullptr;
	hr = thresholdCBV->Map(0, nullptr, reinterpret_cast<void**>(&thresholdData));
	if (FAILED(hr) || thresholdData == nullptr) {
		OutputDebugStringA("[ERROR] Failed to map thresholdCBV\n");
		return false; // または return false;
	}
	*thresholdData = 0.1f; // 初期しきい値（例）

	// 既存と同様にハンドル取得
	D3D12_CPU_DESCRIPTOR_HANDLE receivedSrvHandleCPU =
		GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 4);
	D3D12_GPU_DESCRIPTOR_HANDLE receivedSrvHandleGPU =
		GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 4);
	CreateTextureSRV(device.Get(), texture.Get(),
		receivedSrvHandleCPU); // SRVは1回作成でOK

	//**************************
	// XAudio2の初期化
	//**************************
	ComPtr<IXAudio2> xAudio2;
	IXAudio2MasteringVoice* masterVoice;

	// XAudioエンジンのインスタンスを生成
	HRESULT result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	result = xAudio2->CreateMasteringVoice(&masterVoice);

	// 音声読み込み
	SoundData soundData1 = SoundLoadWave("Resources/Alarm01.wav");

	// 音声再生
	SoundPlayWave(xAudio2.Get(), soundData1);
	DebugCamera debugCamera;
	debugCamera.Initialize();

	////**************************
	//// DirectInputの初期化
	////**************************
	//IDirectInput8* directInput = nullptr;
	//result =
	//	DirectInput8Create(wc.hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8,
	//		(void**)&directInput, nullptr);
	//assert(SUCCEEDED(result));

	//// キーボードデバイスの生成
	//IDirectInputDevice8* keyboard = nullptr;
	//result = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
	//assert(SUCCEEDED(result));

	//// 入力データ形式のセット
	//result = keyboard->SetDataFormat(&c_dfDIKeyboard); // 標準形式
	//assert(SUCCEEDED(result));

	//// 排他制御レベルのセット
	//result = keyboard->SetCooperativeLevel(
	//	hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	//assert(SUCCEEDED(result));
	// どこかの初期化で一度
	CpuTransferCtx xferCtx;

	// ReceiverManager receiver(12345); // 受信ポート
	// receiver.Start();

	// TextureUploader uploader;

	MSG msg{};
	// ウィンドウのxボタンが押されるまでループ
	while (msg.message != WM_QUIT) {
		// windowsにメッセージが来てたら最優先で処理させる
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {

			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			/*UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();*/
			UINT backBufferIndex = swapChain.CurrentIndex();

			// コマンドアロケータとコマンドリストをReset
			ComPtr<ID3D12GraphicsCommandList> commandList = clPool.Begin(backBufferIndex, /*初期PSO*/ graphicsPipelineState.Get());

			//**************************
			// TransitionBarrierを張る
			//**************************
			D3D12_RESOURCE_BARRIER barrier{};

			// バリア
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

			// noneにする
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

			// 現在のバックバッファに対してバリアをはる
			//barrier.Transition.pResource = swapChainResources[backBufferIndex].Get();
			ID3D12Resource* backBuffer = swapChain.BackBuffer(backBufferIndex);
			barrier.Transition.pResource = backBuffer;


			// 遷移前(現在)のResourceState
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;

			// 遷移後のResourceState
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

			// TransitionBarrierを張る
			commandList->ResourceBarrier(1, &barrier);

			// 描画先のRTVとDSVを設定する
			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
				dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			D3D12_CPU_DESCRIPTOR_HANDLE rtv = swapChain.RTV(backBufferIndex);
			commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsvHandle); // 第3引数は BOOL


			float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };
			/*commandList->ClearRenderTargetView(rtvHandles[backBufferIndex],
				clearColor, 0, nullptr);*/
			commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
			commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH,
				1.0f, 0, 0, nullptr);

			//**************************
			// ゲームの処理
			//**************************

			// Y軸回転を加算
			// transform.rotate.y += 0.01f;

			// キーボードの
			//keyboard->Acquire();

			// 全キーの状態を格納する配列（0〜255）
			BYTE key[256] = {};

			// キーボードの入力状態を取得
			//keyboard->GetDeviceState(sizeof(key), key);

			//**************************
			// デコードされた映像取得と解析
			//**************************

			bool gotNewFrame = GrabFrameToD3D12_RGB(
				device.Get(),
				commandList.Get(),
				commandQueue.Get(),
				fence.Get(),
				fenceEvent,
				fenceValue,
				texture.Get(),
				width, height,
				frameQueue,
				frameQueueMtx);

			/*if (gotNewFrame)
				OutputDebugStringA("[OK ] New frame received and copied\n");
			else
				OutputDebugStringA("[WARN] No frame available, skipping update\n");*/

				//**************************
				// 描画処理
				//**************************

				//**************************
				// 座標変換行列を作成
				//**************************
			Matrix4x4 worldMatrix = MakeAffineMatrix(
				transform.scale, transform.rotate, transform.translate);
			Matrix4x4 viewMatrix;
			Matrix4x4 projMatrix;
			debugCamera.Update();
			viewMatrix = debugCamera.GetViewMatrix();
			projMatrix = debugCamera.GetProjectionMatrix();

			// WVPMatrixを作る
			Matrix4x4 worldViewProjectionMatrix =
				Multiply(worldMatrix, Multiply(viewMatrix, projMatrix));

			// CBufferへ書き込む
			*wvpData = worldViewProjectionMatrix;

			// Sprite用のWorldViewProjectionMatrixを作る
			Matrix4x4 worldMatrixSprite =
				MakeAffineMatrix(transformSprite.scale, transformSprite.rotate,
					transformSprite.translate);
			Matrix4x4 viewMatrixSprite = MakeIdentity4x4();
			Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(
				0.0f, 0.0f, float(wd.width), float(wd.height), 0.0f, 100.0f);
			Matrix4x4 worldViewProjectionMatrixSprite =
				Multiply(worldMatrixSprite,
					Multiply(viewMatrixSprite, projectionMatrixSprite));
			*transformationMatrixDataSprite = worldViewProjectionMatrixSprite;

			Matrix4x4 uvTransformMatrix = MakeScaleMatrix(uvTransformSprite.scale);
			uvTransformMatrix = Multiply(
				uvTransformMatrix, MakeRoateZMatrix(uvTransformSprite.rotate.z));
			uvTransformMatrix = Multiply(
				uvTransformMatrix, MakeTranslateMatrix(uvTransformSprite.translate));
			materialDataSprite->uvTransform = uvTransformMatrix;

			Matrix4x4 worldMatrixSphere = MakeAffineMatrix(
				transform.scale, transform.rotate, transform.translate);
			Matrix4x4 viewMatrixSphere =
				debugCamera.GetViewMatrix(); // viewMatrix を取得
			Matrix4x4 projectionMatrixSphere =
				debugCamera.GetProjectionMatrix(); // projectionMatrix を取得
			Matrix4x4 wvpSphere = Multiply(
				worldMatrixSphere, Multiply(viewMatrixSphere, projectionMatrix));

			if (sphere.mappedCBV) {
				sphere.mappedCBV->WVP = wvpSphere;
				sphere.mappedCBV->World = Transpose(Inverse(worldMatrixSphere));
			}

			// --- ImGuiでUI構築 ---
			ImGui::ShowDemoWindow(); // または自作UI

			// 色を変更（RGBA）
			ImGui::ColorEdit3("Light Color",
				reinterpret_cast<float*>(&directionalLightData.color));

			// 向きを変更（任意のベクトル、後で正規化）
			ImGui::SliderFloat3(
				"Light Direction",
				reinterpret_cast<float*>(&directionalLightData.direction), -1.0f,
				1.0f);

			// 輝度を調整（例：0.0〜10.0）
			ImGui::SliderFloat("Intensity", &directionalLightData.intensity, 0.0f,
				10.0f);


			ImGui::Begin("Material Settings");
			ImGui::ColorEdit4("Material Color",
				reinterpret_cast<float*>(materialData));
			ImGui::Text("Scale");
			ImGui::DragFloat3("Scale", reinterpret_cast<float*>(&transform.scale),
				0.01f, 0.01f, 10.0f);

			ImGui::Text("Rotate");
			ImGui::DragFloat3("Rotate", reinterpret_cast<float*>(&transform.rotate),
				0.01f, -3.14f, 3.14f);

			ImGui::Text("Translate");
			ImGui::DragFloat3("Translate",
				reinterpret_cast<float*>(&transform.translate), 0.01f,
				-100.0f, 100.0f);

			ImGui::DragFloat2("UVTranslate", &uvTransformSprite.translate.x, 0.01f,
				-10.0f, 10.0f);
			ImGui::DragFloat2("UVScale", &uvTransformSprite.scale.x, 0.01f, -10.0f,
				10.0f);
			ImGui::SliderAngle("UVRotate", &uvTransformSprite.rotate.z);


			ImGui::End();

			// --- ImGui描画準備 ---
			ImGui::Render();

			// 向きベクトルは単位ベクトルに正規化
			directionalLightData.direction =
				Normalize(directionalLightData.direction);
			*mappedLight = directionalLightData;

			//**************************
			// 描画に必要なステート設定
			//**************************

			// Viewportを設定
			commandList->RSSetViewports(1, &viewport);

			// Scissorを設定
			commandList->RSSetScissorRects(1, &scissorRect);

			// RootSignatureを設定。PSOに設定しているが明示的に設定が必要
			commandList->SetGraphicsRootSignature(rootSignature.Get());

			// PSOを設定
			commandList->SetPipelineState(graphicsPipelineState.Get());

			//***************************
			// 描画コマンドを積む
			//***************************

			// IBVを設定
			commandList->IASetIndexBuffer(&indexBufferViewSprite);

			// トポロジ（図形の形）を指定。三角形リスト（3頂点1面）を指定
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			// マテリアル用CBufferの場所を設定
			// commandList->SetGraphicsRootConstantBufferView(0,
			// materialResource->GetGPUVirtualAddress());

			// マテリアル用CBufferの場所を設定
			commandList->SetGraphicsRootConstantBufferView(
				0, materialResourceSprite->GetGPUVirtualAddress());

			// wvp用のCBufferの場所を設定
			commandList->SetGraphicsRootConstantBufferView(
				1, wvpResource->GetGPUVirtualAddress());

			// light用のCBufferの場所を設定
			commandList->SetGraphicsRootConstantBufferView(
				3, directionalLightResource->GetGPUVirtualAddress());

			ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap.Get() };
			commandList->SetDescriptorHeaps(1, descriptorHeaps);

			// DescriptorTableを設定する
			commandList->SetGraphicsRootDescriptorTable(
				2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);

			// 描画！（ドローコール）3頂点で1つのインスタンスを描画
			// commandList->DrawInstanced(6, 1, 0, 0);

			//commandList->SetGraphicsRootDescriptorTable(2, receivedSrvHandleGPU);

			// commandList->SetGraphicsRootDescriptorTable(2,
			//                                             motionMaskSRVHandle); //
			//                                             t2用

			commandList->DrawInstanced(6, 1, 0, 0); // フルスクリーン矩形に描画

			// 描画！（DrawCall／ドローコール）6個のインデックスを使用し1つのインスタンスを描画。その他は当面0で良い
			commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

			// Spriteの描画。変更が必要なものだけ変更する

			// VBVを設定
			commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite);

			// TransformationMatrixCBufferの場所を設定
			commandList->SetGraphicsRootConstantBufferView(
				1, transformationMatrixResourceSprite->GetGPUVirtualAddress());

			// 描画！（DrawCall/ドローコール）
			commandList->DrawInstanced(6, 1, 0, 0);

			// 安全にステートを再設定してから球を描画
			commandList->IASetVertexBuffers(0, 1, &sphere.vbv);

			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			// --- 球用ステートを明示的に再設定 ---
			commandList->SetGraphicsRootConstantBufferView(
				0, materialResource->GetGPUVirtualAddress());
			commandList->SetGraphicsRootDescriptorTable(
				2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);
			commandList->SetGraphicsRootConstantBufferView(
				3, directionalLightResource->GetGPUVirtualAddress());

			// ImGuiの描画コマンドをコマンドリストに積む
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());

			{
				D3D12_RESOURCE_BARRIER b{};
				b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				b.Transition.pResource = backBuffer;
				b.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
				b.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
				b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				commandList->ResourceBarrier(1, &b);
			}


			/*hr = commandList->Close();
			if (FAILED(hr)) {
				char msg[256];
				sprintf_s(msg, "[ERROR] commandList->Close() failed: 0x%08X\n", hr);
				OutputDebugStringA(msg);
			}
			assert(SUCCEEDED(hr));*/

			//**************************
			// 描画コマンド終了
			//**************************
			/*ID3D12CommandList* commandLists[] = { commandList.Get() };
			commandQueue->ExecuteCommandLists(1, commandLists);*/

			clPool.EndAndExecute(dev);

			//barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			//barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

			//std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);

			//// TransitionBarrierを張る
			//commandList->ResourceBarrier(1, &barrier);

			//swapChain->Present(1, 0);
			swapChain.Present(dev, 1);

			// Fenceの値を更新
			fenceValue++;

			// GPUがここまでたどり着いた時に,Fenceの値を指定した値に代入するようにSignalを送る
			commandQueue->Signal(fence.Get(), fenceValue);

			// GetCompleteValueの初期値はFence作成時に渡した初期値
			if (fence->GetCompletedValue() < fenceValue) {

				// 指定したSignalにたどり着いていないので、たどり着くまで待つようにイベントを設定する
				fence->SetEventOnCompletion(fenceValue, fenceEvent);

				// イベントを待つ
				WaitForSingleObject(fenceEvent, INFINITE);
			}

			/*hr = commandAllocator->Reset();
			assert(SUCCEEDED(hr));
			hr = commandList->Reset(commandAllocator.Get(),
				graphicsPipelineState.Get());
			assert(SUCCEEDED(hr));*/
		}
	}

	MFShutdown();

	//**************************
	// logを出す
	//**************************
	// 現在時刻を取得
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
		nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
	std::chrono::zoned_time localTime{ std::chrono::current_zone(), nowSeconds };
	std::string dateString = std::format("{:%Y%m%d_%H%M%S}", localTime);
	std::string logFilePath = std::string("logs/") + dateString + "log";
	std::ofstream logStream(logFilePath);

	// ImGuiの終了処理
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	//**************************
	// DirectX12のオブジェクトを解放
	//**************************

	CloseHandle(fenceEvent);

	/*if (fence)
	{
			fence->Release();
			fence = nullptr;
	}*/

	/*if (rtvDescriptorHeap)
	{
			rtvDescriptorHeap->Release();
			rtvDescriptorHeap = nullptr;
	}*/

	/*if (commandList)
	{
			commandList->Release();
			commandList = nullptr;
	}*/

	/*if (commandAllocator)
	{
			commandAllocator->Release();
			commandAllocator = nullptr;
	}*/

	/*if (commandQueue) {
			commandQueue->Release();
			commandQueue = nullptr;
	}*/

	// if (rtvDescriptorHeap)
	//{
	//	rtvDescriptorHeap->Release();
	//	rtvDescriptorHeap = nullptr;
	// }

	/*for (int i = 0; i < 2; ++i)
	{
			if (swapChainResources[i])
			{
					swapChainResources[i]->Release();
					swapChainResources[i] = nullptr;
			}
	}*/

	/*if (swapChain)
	{
			swapChain->Release();
			swapChain = nullptr;
	}*/

	/*if (device)
	{
			device->Release();
			device = nullptr;
	}*/

	/*if (useAdapter)
	{
			useAdapter->Release();
			useAdapter = nullptr;
	}*/

	/*if (dxgiFactory)
	{
			dxgiFactory->Release();
			dxgiFactory = nullptr;
	}*/

	/*vertexResourceSphere->Release();*/

	// --- sphereの解放（最後の解放処理群に追加） ---
	/*if (sphere.vertexResource) {
			sphere.vertexResource->Release();
			sphere.vertexResource = nullptr;
	}
	if (sphere.cbvResource) {
			sphere.cbvResource->Release();
			sphere.cbvResource = nullptr;
	}*/

	includeHandler->Release();
	dxcCompiler->Release();
	dxcUtils->Release();

	// graphicsPipelineState->Release();
	signatureBlob->Release();

	if (errorBlob) {
		errorBlob->Release();
	}

	// rootSignature->Release();
	pixelShaderBlob->Release();
	vertexShaderBlob->Release();

	// XAudio2 解放
	xAudio2.Reset();

	// 音声データ解放
	SoundUnload(&soundData1);

	// materialResource->Release();

#ifdef _DEBUG
	ComPtr<ID3D12Debug1> debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		debugController->EnableDebugLayer();

		debugController->SetEnableGPUBasedValidation(TRUE);
	}
	debugController.Reset();
#endif
	CloseWindow(hwnd);

	/*IDXGIDebug* debug;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug))))
	{
			debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);

			debug->Release();
	}*/
	encoder.Shutdown();
	// 終了時
	stopRequested = true;
	if (th.joinable()) th.join();
	/*
	WSACleanup();
	*/
	// comの終了処理
	CoUninitialize();
	return 0;
}