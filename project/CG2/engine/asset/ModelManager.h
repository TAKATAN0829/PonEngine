#pragma once

#include "AssetPathChange.h"

// C++
#include <string>
#include <map>
#include <memory>
#include <vector>

// 自作
#include "Model.h"

class DirectXCommon;
class SrvManager;
class ModelCommon;

class ModelManager {
public:

	//=============================================================================================//
	// インスタンス取得

	static ModelManager* GetInstance ();

	//=============================================================================================//
	// 初期化処理

	void Initialize (DirectXCommon* dxCommon, SrvManager* srvManager);

	//=============================================================================================//
	// 終了処理

	void Finalize ();

	//=============================================================================================//
	// モデルファイル読み込み

	void LoadModel (const std::string& filePath);

	// インポート失敗をエラーとして返し、未完成のモデルをキャッシュしない。
	bool TryLoadModel(const std::string& filePath, std::string& error);

	//=============================================================================================//
	// Asset参照の更新

	void RemapAssetPaths(const AssetPathChange& change);

	//=============================================================================================//
	// モデル検索

	Model* FindModel (const std::string& filePath);

private:

	ModelManager () = default;

	~ModelManager () = default;

	ModelManager (const ModelManager&) = delete;

	ModelManager& operator=(const ModelManager&) = delete;

private:

	//=============================================================================================//
	// モデル格納ディレクトリ

	inline static const std::string kDirectoryPath = "resources";

	//=============================================================================================//
	// ModelCommon

	ModelCommon* modelCommon_ = nullptr;

	//=============================================================================================//
	// モデルデータ

	std::map<std::string, std::unique_ptr<Model>> models_;

	// 同じ移動先の古いキャッシュは、描画中の借用ポインタを壊さず終了時に解放する。
	std::vector<std::unique_ptr<Model>> retiredModels_;
};
