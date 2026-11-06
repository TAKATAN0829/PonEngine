#pragma once

#include <cstring>
#include <string>

// ImGuiにコピーして渡す固定長データ。アセット一覧の借用ポインタを持ち越さない。
struct ModelAssetPayload {
	inline static constexpr const char* kType = "PON_MODEL_ASSET";
	inline static constexpr const char* kTextureType = "PON_TEXTURE_ASSET";
	inline static constexpr const char* kPrefabType = "PON_PREFAB_ASSET";
	inline static constexpr const char* kProjectType = "PON_PROJECT_ASSET";

	char model[256];

	static bool Read(const void* data, size_t size, std::string& result);
};
