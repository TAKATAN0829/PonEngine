#include "AssetDragPayload.h"

//=============================================================================================//
// 共通処理

bool ModelAssetPayload::Read(const void* data, size_t size, std::string& result) {
	// 受け取ったデータのサイズと終端を検証して、Assetのパスを取り出す。
	if (data == nullptr) {
		return false;
	}

	// 固定長の構造体と同じサイズを受け取った場合だけコピーする。
	if (size != sizeof(ModelAssetPayload)) {
		return false;
	}

	ModelAssetPayload payload;

	std::memcpy(&payload, data, sizeof(payload));

	// 配列の中に文字列の終端があることを確認し、範囲外の読み取りを防ぐ。
	if (std::memchr(payload.model, '\0', sizeof(payload.model)) == nullptr) {
		return false;
	}

	result = payload.model;

	// 空のパスは配置や移動の対象として使えない。
	if (!result.empty()) {
		return true;
	}

	return false;
}
