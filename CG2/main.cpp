#pragma warning(push)
// C4023の警告をみなかったことにする
#pragma warning(disable:4023)

#include <Windows.h>
#include <cstdint>
#include <string>
#include <format>
#include <fstream>
#include <cassert>
#include <vector>
#include <sstream>
#include <cstring>
#include <numbers>

#include <d3d12.h>
#pragma comment(lib,"d3d12.lib")

#include <dxgi1_6.h>
#pragma comment(lib,"dxgi.lib")

#include <dxgidebug.h>
#pragma comment(lib,"dxguid.lib")

#include <dxcapi.h>
#pragma comment(lib,"dxcompiler.lib")

#include <xaudio2.h>
#pragma comment(lib,"xaudio2.lib")

#include "../externals/DirectXTex/DirectXTex.h"
#include "../externals/DirectXTex/d3dx12.h"

// ImGui
#ifdef USE_IMGUI
#include "../externals/imgui/imgui.h"
#include "../externals/imgui/imgui_impl_dx12.h"
#include "../externals/imgui/imgui_impl_win32.h"
#endif

#pragma warning(pop)

#include "MathUtility.h"
#include "WinApp.h"
#include "DirectXCommon.h"
#include "LogSystem.h"
#include "Input.h"
#include "GraphicsSystem.h"

// Transform情報
struct TransformData {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

// 頂点データ
struct VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
};

// マテリアル
struct Material {
	Vector4 color;
	int32_t enableLighting;
	float padding[3];
	Matrix4x4 uvTransform;
};

// TransformationMatrix
struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
};

// 平行光源
struct DirectionalLight {
	Vector4 color;
	Vector3 direction;
	float intensity;
};

// モデルデータ
struct ModelData {
	std::vector<VertexData> vertices;
};

// リソースチェッカー
struct D3DResourceLeakChecker {
	~D3DResourceLeakChecker () {

		Microsoft::WRL::ComPtr<IDXGIDebug1> debug;

		if (SUCCEEDED (DXGIGetDebugInterface1 (0, IID_PPV_ARGS (&debug)))) {

			debug->ReportLiveObjects (DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects (DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects (DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
		}
	}
};

// チャンクヘッダ
struct ChunkHeader {
	char id[4];
	int32_t size;
};

// RIFFヘッダチャンク
struct RiffHeader {
	ChunkHeader chunk;
	char type[4];
};

// FMTチャンク
struct FormatChunk {
	ChunkHeader chunk;
	WAVEFORMATEX fmt;
};

// 音声データ
struct SoundData {
	WAVEFORMATEX wfex;
	BYTE* pBuffer;
	unsigned int bufferSize;
};

const uint32_t kSubdivision = 16;
const uint32_t kVertexCount = (kSubdivision + 1) * (kSubdivision + 1);
const uint32_t kIndexCount = kSubdivision * kSubdivision * 6;

//=================================================================================================//
// TextureResourceにデータを転送

[[nodiscard]]
Microsoft::WRL::ComPtr<ID3D12Resource>
UploadTextureData (
	const Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
	const DirectX::ScratchImage& mipImages,
	ID3D12Device* device,
	ID3D12GraphicsCommandList* commandList,
	ResourceManager* resourceManager) {

	// SubResource作成
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;

	DirectX::PrepareUpload (
		device,
		mipImages.GetImages (),
		mipImages.GetImageCount (),
		mipImages.GetMetadata (),
		subresources);

	// IntermediateResourceのサイズ取得
	uint64_t intermediateSize =
		GetRequiredIntermediateSize (texture.Get (), 0, UINT (subresources.size ()));

	// IntermediateResource生成
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource =
		resourceManager->CreateBufferResource (
			device,
			intermediateSize,
			"intermediateResource");

	// Textureに転送
	UpdateSubresources (
		commandList,
		texture.Get (),
		intermediateResource.Get (),
		0,
		0,
		UINT (subresources.size ()),
		subresources.data ());

	// ResourceBarrier設定
	D3D12_RESOURCE_BARRIER barrier{};

	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

	barrier.Transition.pResource = texture.Get ();

	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;

	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;

	// ResourceBarrierを張る
	commandList->ResourceBarrier (1, &barrier);

	return intermediateResource;
}

//=================================================================================================//
// OBJファイルを読み込む

ModelData LoadObjFile (
	const std::string& directoryPath,
	const std::string& filename) {

	ModelData modelData;

	std::vector<Vector4> positions;
	std::vector<Vector3> normals;
	std::vector<Vector2> texcoords;

	std::string line;

	// ファイルを開く
	std::ifstream file (directoryPath + "/" + filename);

	assert (file.is_open ());

	LogSystem::Log (filename + " is opened!!\n");

	// ファイルを読む
	while (std::getline (file, line)) {

		std::string identifier;

		std::istringstream s (line);

		s >> identifier;

		if (identifier == "v") {

			Vector4 position{};

			s >> position.x >> position.y >> position.z;

			position.x *= -1.0f;
			position.w = 1.0f;

			positions.push_back (position);

		} else if (identifier == "vt") {

			Vector2 texcoord{};

			s >> texcoord.x >> texcoord.y;

			texcoord.y = 1.0f - texcoord.y;

			texcoords.push_back (texcoord);

		} else if (identifier == "vn") {

			Vector3 normal{};

			s >> normal.x >> normal.y >> normal.z;

			normal.x *= -1.0f;

			normals.push_back (normal);

		} else if (identifier == "f") {

			VertexData triangle[3]{};

			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {

				std::string vertexDefinition;

				s >> vertexDefinition;

				std::istringstream v (vertexDefinition);

				uint32_t elementIndices[3]{};

				for (int32_t element = 0; element < 3; ++element) {

					std::string index;

					std::getline (v, index, '/');

					elementIndices[element] =
						std::stoi (index);
				}

				Vector4 position =
					positions[elementIndices[0] - 1];

				Vector2 texcoord =
					texcoords[elementIndices[1] - 1];

				Vector3 normal =
					normals[elementIndices[2] - 1];

				triangle[faceVertex] = { position,texcoord,	normal };
			}

			// 頂点を逆順で登録して回り順を逆にする
			modelData.vertices.push_back (triangle[2]);
			modelData.vertices.push_back (triangle[1]);
			modelData.vertices.push_back (triangle[0]);
		}
	}

	return modelData;
}

//=================================================================================================//
// 音声データの読み込み

SoundData SoundLoadWave (const char* filename) {

	// ファイルを開く
	std::ifstream file;

	file.open (filename, std::ios_base::binary);

	assert (file.is_open ());

	// RIFFヘッダーの読み込み
	RiffHeader riff{};

	file.read (reinterpret_cast<char*>(&riff), sizeof (riff));

	// RIFFかチェック
	if (strncmp (riff.chunk.id, "RIFF", 4) != 0) {
		assert (0);
	}

	// WAVEかチェック
	if (strncmp (riff.type, "WAVE", 4) != 0) {
		assert (0);
	}

	// Formatチャンクの読み込み
	FormatChunk format{};

	file.read (reinterpret_cast<char*>(&format), sizeof (ChunkHeader));

	if (strncmp (format.chunk.id, "fmt ", 4) != 0) {
		assert (0);
	}

	assert (format.chunk.size <= sizeof (format.fmt));

	file.read (reinterpret_cast<char*>(&format.fmt), format.chunk.size);

	// Dataチャンクの読み込み
	ChunkHeader data{};

	file.read (reinterpret_cast<char*>(&data), sizeof (data));

	// JUNKチャンクを検出した場合
	if (strncmp (data.id, "JUNK", 4) == 0) {

		file.seekg (data.size, std::ios_base::cur);

		file.read (reinterpret_cast<char*>(&data), sizeof (data));
	}

	if (strncmp (data.id, "data", 4) != 0) {
		assert (0);
	}

	// 波形データの読み込み
	char* pBuffer = new char[data.size];

	file.read (pBuffer, data.size);

	file.close ();

	SoundData soundData{};

	soundData.wfex = format.fmt;
	soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
	soundData.bufferSize = data.size;

	return soundData;
}

//=================================================================================================//
// 音声データの解放

void SoundUnload (SoundData* soundData) {

	delete[] soundData->pBuffer;

	soundData->pBuffer = nullptr;
	soundData->bufferSize = 0;
	soundData->wfex = {};
}

//=================================================================================================//
// サウンドの再生

void SoundPlayWave (
	IXAudio2* xAudio2,
	const SoundData& soundData) {

	HRESULT result;

	// SourceVoice生成
	IXAudio2SourceVoice* pSourceVoice = nullptr;

	result = xAudio2->CreateSourceVoice (&pSourceVoice, &soundData.wfex);

	assert (SUCCEEDED (result));

	// 再生する波形データの設定
	XAUDIO2_BUFFER buf{};

	buf.pAudioData = soundData.pBuffer;
	buf.AudioBytes = soundData.bufferSize;
	buf.Flags = XAUDIO2_END_OF_STREAM;

	// 再生
	result = pSourceVoice->SubmitSourceBuffer (&buf);

	assert (SUCCEEDED (result));

	result = pSourceVoice->Start ();

	assert (SUCCEEDED (result));
}

//=================================================================================================//
// Windowsアプリでのエントリーポイント

int WINAPI WinMain (_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

	//=============================================================================================//
	// リソースリークチェッカー

	D3DResourceLeakChecker leakChecker;

	//=============================================================================================//
	// COM初期化

	CoInitializeEx (0, COINIT_MULTITHREADED);

	//=============================================================================================//
	// LogSystem初期化

	LogSystem::Initialize ();

	//=============================================================================================//
	// Window初期化

	WinApp* winApp = new WinApp ();

	winApp->Initialize ();

	//=============================================================================================//
	// DirectXCommon初期化

	DirectXCommon* dxCommon = new DirectXCommon ();

	dxCommon->Initialize (
		winApp->GetHwnd (),
		winApp->GetClientWidth (),
		winApp->GetClientHeight ());

	//=============================================================================================//
	// Input初期化

	Input* input = new Input ();

	input->Initialize (
		GetModuleHandle (nullptr),
		winApp->GetHwnd ());

	//=============================================================================================//
	// CommandList取得

	ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList ();

	//=============================================================================================//
	// Device取得

	ID3D12Device* device = dxCommon->GetDevice ();

	//=============================================================================================//
	// GraphicsSystem初期化

	GraphicsSystem* graphicsSystem = new GraphicsSystem ();

	graphicsSystem->Initialize (
		device);

	//=============================================================================================//
	// GraphicsSystem内のManager取得

	DescriptorHeapManager* srvDescriptorHeap =
		graphicsSystem->GetSRVDescriptorHeap ();

	ShaderCompiler* shaderCompiler =
		graphicsSystem->GetShaderCompiler ();

	TextureManager* textureManager =
		graphicsSystem->GetTextureManager ();

	ResourceManager* resourceManager =
		graphicsSystem->GetResourceManager ();

	PipelineManager* pipelineManager =
		graphicsSystem->GetPipelineManager ();

	//=============================================================================================//
	// VertexResource生成

	Microsoft::WRL::ComPtr<ID3D12Resource>	vertexResource =
		resourceManager->CreateBufferResource (
			device,
			sizeof (VertexData) * kVertexCount,
			"vertexResource");

	//=============================================================================================//
	// VertexBufferView作成

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};

	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress ();

	vertexBufferView.SizeInBytes = sizeof (VertexData) * kVertexCount;

	vertexBufferView.StrideInBytes = sizeof (VertexData);

	//=============================================================================================//
	// VertexDataを書き込む

	VertexData* vertexData = nullptr;

	vertexResource->Map (0, nullptr, reinterpret_cast<void**>(&vertexData));

	//=============================================================================================//
	// Sphere生成

	const float kLonEvery = std::numbers::pi_v<float> *2.0f / float (kSubdivision);

	const float kLatEvery = std::numbers::pi_v<float> / float (kSubdivision);

	for (uint32_t latIndex = 0; latIndex <= kSubdivision; ++latIndex) {

		float lat = -std::numbers::pi_v<float> / 2.0f + kLatEvery * latIndex;

		for (uint32_t lonIndex = 0; lonIndex <= kSubdivision; ++lonIndex) {

			uint32_t index = latIndex * (kSubdivision + 1) + lonIndex;

			float lon = lonIndex * kLonEvery;

			vertexData[index].position.x = cos (lat) * cos (lon);
			vertexData[index].position.y = sin (lat);
			vertexData[index].position.z = cos (lat) * sin (lon);
			vertexData[index].position.w = 1.0f;
			vertexData[index].texcoord = { float (lonIndex) / float (kSubdivision),	1.0f - float (latIndex) / float (kSubdivision) };
			vertexData[index].normal.x = vertexData[index].position.x;
			vertexData[index].normal.y = vertexData[index].position.y;
			vertexData[index].normal.z = vertexData[index].position.z;
		}
	}

	//=============================================================================================//
	// IndexResource生成

	Microsoft::WRL::ComPtr<ID3D12Resource>	indexResource =
		resourceManager->CreateBufferResource (
			device,
			sizeof (uint32_t) * kIndexCount,
			"indexResource");

	//=============================================================================================//
	// IndexBufferView作成

	D3D12_INDEX_BUFFER_VIEW indexBufferView{};

	indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress ();

	indexBufferView.SizeInBytes = sizeof (uint32_t) * kIndexCount;

	indexBufferView.Format = DXGI_FORMAT_R32_UINT;

	//=============================================================================================//
	// IndexDataを書き込む

	uint32_t* indexData = nullptr;

	indexResource->Map (0, nullptr, reinterpret_cast<void**>(&indexData));

	for (uint32_t latIndex = 0; latIndex < kSubdivision; ++latIndex) {

		for (uint32_t lonIndex = 0; lonIndex < kSubdivision; ++lonIndex) {

			uint32_t start = (latIndex * (kSubdivision + 1)) + lonIndex;
			uint32_t index = (latIndex * kSubdivision + lonIndex) * 6;

			indexData[index + 0] = start;
			indexData[index + 1] = start + kSubdivision + 1;
			indexData[index + 2] = start + 1;

			indexData[index + 3] = start + 1;
			indexData[index + 4] = start + kSubdivision + 1;
			indexData[index + 5] = start + kSubdivision + 2;
		}
	}

	//=============================================================================================//
	// MaterialResource生成

	Microsoft::WRL::ComPtr<ID3D12Resource>	materialResource =
		resourceManager->CreateBufferResource (
			device,
			sizeof (Material),
			"materialResource");

	//=============================================================================================//
	// MaterialDataを書き込む

	Material* materialData = nullptr;

	materialResource->Map (0, nullptr, reinterpret_cast<void**>(&materialData));

	materialData->color = { 1.0f,1.0f,1.0f,1.0f };

	materialData->enableLighting = true;

	materialData->uvTransform = MathUtility::MakeIdentity4x4 ();

	//=============================================================================================//
	// WVPResource生成

	Microsoft::WRL::ComPtr<ID3D12Resource>	wvpResource =
		resourceManager->CreateBufferResource (
			device,
			sizeof (TransformationMatrix),
			"wvpResource");

	//=============================================================================================//
	// WVPDataを書き込む

	TransformationMatrix* wvpData = nullptr;

	wvpResource->Map (0, nullptr, reinterpret_cast<void**>(&wvpData));

	wvpData->WVP = MathUtility::MakeIdentity4x4 ();

	wvpData->World = MathUtility::MakeIdentity4x4 ();

	//=============================================================================================//
	// DirectionalLightResource生成

	Microsoft::WRL::ComPtr<ID3D12Resource>	directionalLightResource =
		resourceManager->CreateBufferResource (
			device,
			sizeof (DirectionalLight),
			"directionalLightResource");

	//=============================================================================================//
	// DirectionalLightDataを書き込む

	DirectionalLight* directionalLightData = nullptr;

	directionalLightResource->Map (0, nullptr, reinterpret_cast<void**>(&directionalLightData));

	directionalLightData->color = { 1.0f,1.0f,1.0f,1.0f };

	directionalLightData->direction = { 0.0f,-1.0f,0.0f };

	directionalLightData->intensity = 1.0f;

	//=============================================================================================//
	// Texture読み込み

	DirectX::ScratchImage mipImages =
		textureManager->LoadTexture ("./Resources/uvChecker.png");

	const DirectX::TexMetadata& metadata = mipImages.GetMetadata ();

	//=============================================================================================//
	// TextureResource生成

	Microsoft::WRL::ComPtr<ID3D12Resource>	textureResource =
		resourceManager->CreateTextureResource (
			device,
			metadata);

	//=============================================================================================//
	// Texture転送

	Microsoft::WRL::ComPtr<ID3D12Resource>	intermediateResource =
		UploadTextureData (
			textureResource,
			mipImages,
			device,
			commandList,
			resourceManager);

	//=============================================================================================//
	// SRV設定

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};

	srvDesc.Format = metadata.format;

	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

	srvDesc.Texture2D.MipLevels = UINT (metadata.mipLevels);

	//=============================================================================================//
	// SRV生成

	device->CreateShaderResourceView (
		textureResource.Get (),
		&srvDesc,
		srvDescriptorHeap->GetCPUDescriptorHandle (0));

	//=============================================================================================//
	// ShaderCompile

	IDxcBlob* vertexShaderBlob =
		shaderCompiler->Compile (
			L"Object3d.VS.hlsl",
			L"vs_6_0");

	IDxcBlob* pixelShaderBlob =
		shaderCompiler->Compile (
			L"Object3d.PS.hlsl",
			L"ps_6_0");

	//=============================================================================================//
	// RootSignature作成

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};

	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	//=============================================================================================//
	// DescriptorRange

	D3D12_DESCRIPTOR_RANGE descriptorRange[1]{};

	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	//=============================================================================================//
	// RootParameter

	D3D12_ROOT_PARAMETER rootParameters[4]{};

	// Material
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	// TransformationMatrix
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].Descriptor.ShaderRegister = 0;

	// Texture
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

	// DirectionalLight
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].Descriptor.ShaderRegister = 1;

	descriptionRootSignature.pParameters = rootParameters;

	descriptionRootSignature.NumParameters = _countof (rootParameters);

	//=============================================================================================//
	// Sampler設定

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1]{};

	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	descriptionRootSignature.pStaticSamplers = staticSamplers;

	descriptionRootSignature.NumStaticSamplers = _countof (staticSamplers);

	//=============================================================================================//
	// RootSignature生成

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature =
		pipelineManager->CreateRootSignature (
			device,
			descriptionRootSignature);

	//=============================================================================================//
	// InputLayout設定

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc =
		pipelineManager->CreateInputLayout ();

	//=============================================================================================//
	// BlendState設定

	D3D12_BLEND_DESC blendDesc =
		pipelineManager->CreateBlendState ();

	//=============================================================================================//
	// RasterizerState設定

	D3D12_RASTERIZER_DESC rasterizerDesc =
		pipelineManager->CreateRasterizerState ();

	//=============================================================================================//
	// DepthStencilState設定

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc =
		pipelineManager->CreateDepthStencilState ();

	//=============================================================================================//
	// PSO生成

	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState =
		pipelineManager->CreateGraphicsPipelineState (
			device,
			rootSignature.Get (),
			inputLayoutDesc,
			blendDesc,
			rasterizerDesc,
			depthStencilDesc,
			vertexShaderBlob,
			pixelShaderBlob);

	//=============================================================================================//
	// Transform初期値

	TransformData transform{
		{1.0f,1.0f,1.0f},
		{0.0f,0.0f,0.0f},
		{0.0f,0.0f,0.0f}
	};

	TransformData cameraTransform{
		{1.0f,1.0f,1.0f},
		{0.0f,0.0f,0.0f},
		{0.0f,0.0f,-5.0f}
	};

	//=============================================================================================//
	// メインループ

	while (winApp->ProcessMessage ()) {

		// Input更新
		input->Update ();

		// Frame開始
		dxCommon->BeginFrame ();

		// CommandList取得
		commandList =
			dxCommon->GetCommandList ();

		//=========================================================================================//
		// WVPMatrix作成

		Matrix4x4 worldMatrix = MathUtility::MakeAffineMatrix (transform.scale, transform.rotate, transform.translate);

		Matrix4x4 cameraMatrix = MathUtility::MakeAffineMatrix (cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);

		Matrix4x4 viewMatrix = MathUtility::Inverse (cameraMatrix);

		Matrix4x4 projectionMatrix = MathUtility::MakePerspectiveFovMatrix (0.45f, float (winApp->GetClientWidth ()) / float (winApp->GetClientHeight ()), 0.1f, 100.0f);

		Matrix4x4 worldViewProjectionMatrix = MathUtility::Multiply (worldMatrix, MathUtility::Multiply (viewMatrix, projectionMatrix));

		wvpData->WVP = worldViewProjectionMatrix;

		wvpData->World = worldMatrix;

		//=========================================================================================//
		// 描画設定

		commandList->SetGraphicsRootSignature (rootSignature.Get ());

		commandList->SetPipelineState (graphicsPipelineState.Get ());

		commandList->IASetVertexBuffers (0, 1, &vertexBufferView);

		commandList->IASetIndexBuffer (&indexBufferView);

		commandList->IASetPrimitiveTopology (D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Heap設定
		ID3D12DescriptorHeap* descriptorHeaps[] = {
			srvDescriptorHeap->GetDescriptorHeap ()
		};

		commandList->SetDescriptorHeaps (1, descriptorHeaps);

		// RootParameter設定
		commandList->SetGraphicsRootConstantBufferView (0, materialResource->GetGPUVirtualAddress ());

		commandList->SetGraphicsRootConstantBufferView (1, wvpResource->GetGPUVirtualAddress ());

		commandList->SetGraphicsRootDescriptorTable (2, srvDescriptorHeap->GetGPUDescriptorHandle (0));

		commandList->SetGraphicsRootConstantBufferView (3, directionalLightResource->GetGPUVirtualAddress ());

		// DrawCall
		commandList->DrawIndexedInstanced (kIndexCount, 1, 0, 0, 0);

		// Frame終了
		dxCommon->EndFrame ();
	}

	//=============================================================================================//
	// 終了処理

	graphicsSystem->Finalize ();
	input->Finalize ();
	dxCommon->Finalize ();
	winApp->Finalize ();
	LogSystem::Finalize ();

	delete graphicsSystem;
	delete input;
	delete dxCommon;
	delete winApp;

	CoUninitialize ();

	return 0;
}