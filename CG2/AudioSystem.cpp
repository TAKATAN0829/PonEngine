#include "AudioSystem.h"

//=================================================================================================//
// 初期化処理

void AudioSystem::Initialize () {

	HRESULT result;

	//=============================================================================================//
	// XAudio2生成

	result =
		XAudio2Create (
			&xAudio2_,
			0,
			XAUDIO2_DEFAULT_PROCESSOR);

	assert (SUCCEEDED (result));

	//=============================================================================================//
	// MasterVoice生成

	result =
		xAudio2_->CreateMasteringVoice (
			&masterVoice_);

	assert (SUCCEEDED (result));
}

//=================================================================================================//
// 終了処理

void AudioSystem::Finalize () {

	//=============================================================================================//
	// MasterVoice解放

	if (masterVoice_ != nullptr) {

		masterVoice_->DestroyVoice ();

		masterVoice_ = nullptr;
	}

	//=============================================================================================//
	// XAudio2解放

	if (xAudio2_ != nullptr) {

		xAudio2_->Release ();

		xAudio2_ = nullptr;
	}
}

//=================================================================================================//
// 音声読み込み

SoundData AudioSystem::LoadWave (
	const char* filename) {

	//=============================================================================================//
	// ファイルを開く

	std::ifstream file;

	file.open (
		filename,
		std::ios_base::binary);

	assert (file.is_open ());

	//=============================================================================================//
	// RIFFヘッダー読み込み

	RiffHeader riff{};

	file.read (
		reinterpret_cast<char*>(&riff),
		sizeof (riff));

	//=============================================================================================//
	// RIFFチェック

	if (strncmp (riff.chunk.id, "RIFF", 4) != 0) {

		assert (0);
	}

	//=============================================================================================//
	// WAVEチェック

	if (strncmp (riff.type, "WAVE", 4) != 0) {

		assert (0);
	}

	//=============================================================================================//
	// Formatチャンク読み込み

	FormatChunk format{};

	file.read (
		reinterpret_cast<char*>(&format),
		sizeof (ChunkHeader));

	if (strncmp (format.chunk.id, "fmt ", 4) != 0) {

		assert (0);
	}

	assert (format.chunk.size <= sizeof (format.fmt));

	file.read (
		reinterpret_cast<char*>(&format.fmt),
		format.chunk.size);

	//=============================================================================================//
	// Dataチャンク読み込み

	ChunkHeader data{};

	file.read (
		reinterpret_cast<char*>(&data),
		sizeof (data));

	//=============================================================================================//
	// JUNKチャンク

	if (strncmp (data.id, "JUNK", 4) == 0) {

		file.seekg (
			data.size,
			std::ios_base::cur);

		file.read (
			reinterpret_cast<char*>(&data),
			sizeof (data));
	}

	//=============================================================================================//
	// dataチェック

	if (strncmp (data.id, "data", 4) != 0) {

		assert (0);
	}

	//=============================================================================================//
	// 波形データ読み込み

	char* pBuffer =
		new char[data.size];

	file.read (
		pBuffer,
		data.size);

	file.close ();

	//=============================================================================================//
	// SoundData作成

	SoundData soundData{};

	soundData.wfex =
		format.fmt;

	soundData.pBuffer =
		reinterpret_cast<BYTE*>(pBuffer);

	soundData.bufferSize =
		data.size;

	return soundData;
}

//=================================================================================================//
// 音声解放

void AudioSystem::UnloadWave (
	SoundData* soundData) {

	delete[] soundData->pBuffer;

	soundData->pBuffer = nullptr;

	soundData->bufferSize = 0;

	soundData->wfex = {};
}

//=================================================================================================//
// 音声再生

void AudioSystem::PlayWave (
	const SoundData& soundData) {

	HRESULT result;

	//=============================================================================================//
	// SourceVoice生成

	IXAudio2SourceVoice* sourceVoice =
		nullptr;

	result =
		xAudio2_->CreateSourceVoice (
			&sourceVoice,
			&soundData.wfex);

	assert (SUCCEEDED (result));

	//=============================================================================================//
	// Buffer設定

	XAUDIO2_BUFFER buffer{};

	buffer.pAudioData =
		soundData.pBuffer;

	buffer.AudioBytes =
		soundData.bufferSize;

	buffer.Flags =
		XAUDIO2_END_OF_STREAM;

	//=============================================================================================//
	// Buffer送信

	result =
		sourceVoice->SubmitSourceBuffer (
			&buffer);

	assert (SUCCEEDED (result));

	//=============================================================================================//
	// 再生

	result =
		sourceVoice->Start ();

	assert (SUCCEEDED (result));
}