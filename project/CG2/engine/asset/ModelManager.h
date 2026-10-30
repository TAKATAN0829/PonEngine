#pragma once

// C++
#include <string>
#include <map>
#include <memory>

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
};
