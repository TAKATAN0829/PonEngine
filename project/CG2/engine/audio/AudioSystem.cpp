#include "AudioSystem.h"

//=============================================================================================//
// 初期化処理


void AudioSystem::Initialize () {

	HRESULT result;

	//=============================================================================================//
	// XAudio2生成

	result = XAudio2Create (&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);

	assert (SUCCEEDED (result));

	//=============================================================================================//
	// MasterVoice生成

	result = xAudio2_->CreateMasteringVoice (&masterVoice_);

	assert (SUCCEEDED (result));
}

//=============================================================================================//
// 終了処理


void AudioSystem::Finalize () {

	//=============================================================================================//
	// MasterVoice解放

	// 出力先のボイスを先に破棄してから、XAudio2の本体を解放する。
	if (masterVoice_ != nullptr) {

		masterVoice_->DestroyVoice ();

		masterVoice_ = nullptr;
	}

	//=============================================================================================//
	// XAudio2解放

	xAudio2_.Reset ();
}

//=============================================================================================//
// 音声読み込み


SoundData AudioSystem::LoadWave (const char* filename) {

	//=============================================================================================//
	// ファイルを開く

	std::ifstream file;

	// WAVはバイト単位で構造が決まっているため、改行を変換しないバイナリモードで開く。
	file.open (filename, std::ios_base::binary);

	// ファイルを開けなければ、デバッグ実行ではここで止める。
	assert (file.is_open ());

	//=============================================================================================//
	// RIFFヘッダー読み込み

	// ファイル先頭の識別子やサイズを受け取る領域を用意する。
	RiffHeader riff{};

	// readは指定したバイト数をコピーする。char*に変換し、ヘッダーの領域を受け皿にする。
	file.read (reinterpret_cast<char*>(&riff), sizeof (riff));

	//=============================================================================================//
	// RIFFチェック

	// ヘッダーの最初の4バイトがRIFFと一致するかを調べる。
	if (strncmp (
		riff.chunk.id,
		"RIFF",
		4) != 0) {

		assert (0);
	}

	//=============================================================================================//
	// WAVEチェック

	// RIFFの中身がWAVであることを、続く種類の欄で確認する。
	if (strncmp (
		riff.type,
		"WAVE",
		4) != 0) {

		assert (0);
	}

	//=============================================================================================//
	// Formatチャンク読み込み

	FormatChunk format{};

	// まずfmtチャンクの種類とサイズを読み、続くフォーマット情報の長さを知る。
	file.read (reinterpret_cast<char*>(&format), sizeof (ChunkHeader));

	if (strncmp (
		format.chunk.id,
		"fmt ",
		4) != 0) {

		assert (0);
	}

	// ファイルが示すサイズが、受け皿の構造体に収まることを確認する。
	assert (format.chunk.size <= sizeof (format.fmt));

	// サンプルレートなどの形式情報を、ヘッダーに書かれた長さだけ読む。
	file.read (reinterpret_cast<char*>(&format.fmt), format.chunk.size);

	//=============================================================================================//
	// Dataチャンク読み込み

	ChunkHeader data{};

	// 形式情報の直後にあるチャンクを見て、波形か読み飛ばす情報かを判別する。
	file.read (reinterpret_cast<char*>(&data), sizeof (data));

	//=============================================================================================//
	// JUNKチャンク

	if (strncmp (
		data.id,
		"JUNK",
		4) == 0) {

		// JUNKは音声データに使わない。現在位置からそのサイズ分進み、次のヘッダーを読む。
		file.seekg (data.size, std::ios_base::cur);

		file.read (reinterpret_cast<char*>(&data), sizeof (data));
	}

	//=============================================================================================//
	// dataチェック

	// ここでdataと確認できたチャンクの中身を、次に波形として読む。
	if (strncmp (
		data.id,
		"data",
		4) != 0) {

		assert (0);
	}

	//=============================================================================================//
	// 波形データ読み込み

	// 波形のサイズはファイルごとに違うため、dataヘッダーの長さで領域を確保する。
	char* pBuffer = new char[data.size];

	// dataチャンクに記録されたサイズだけ、波形をメモリへコピーする。
	file.read (pBuffer, data.size);

	// 必要なバイトを読み込んだので、ファイルを閉じる。
	file.close ();

	//=============================================================================================//
	// SoundData作成

	SoundData soundData{};

	// 再生側が波形を解釈できるよう、読み取った音声の形式も渡す。
	soundData.wfex = format.fmt;

	// ファイルを閉じた後も再生できるよう、波形バッファをSoundDataに引き渡す。
	soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);

	soundData.bufferSize = data.size;

	return soundData;
}

//=============================================================================================//
// 音声解放


void AudioSystem::UnloadWave (SoundData* soundData) {
	// 読み込んだ波形データを解放し、保持していた情報を消す。

	delete[] soundData->pBuffer;

	soundData->pBuffer = nullptr;

	soundData->bufferSize = 0;

	soundData->wfex = {};
}

//=============================================================================================//
// 音声再生


void AudioSystem::PlayWave (const SoundData& soundData) {

	HRESULT result;

	//=============================================================================================//
	// SourceVoice生成

	IXAudio2SourceVoice* sourceVoice = nullptr;

	result = xAudio2_->CreateSourceVoice (&sourceVoice, &soundData.wfex);

	assert (SUCCEEDED (result));

	//=============================================================================================//
	// Buffer設定

	XAUDIO2_BUFFER buffer{};

	buffer.pAudioData = soundData.pBuffer;

	buffer.AudioBytes = soundData.bufferSize;

	buffer.Flags = XAUDIO2_END_OF_STREAM;

	//=============================================================================================//
	// Buffer送信

	result = sourceVoice->SubmitSourceBuffer (&buffer);

	assert (SUCCEEDED (result));

	//=============================================================================================//
	// 再生

	result = sourceVoice->Start ();

	assert (SUCCEEDED (result));
}
