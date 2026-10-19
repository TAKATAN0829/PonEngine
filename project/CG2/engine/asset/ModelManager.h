#pragma once
#include <stdlib.h>
#include <string>
#include <map>
#include <memory>

class ModelManager
{
public:
	// 初期化
	void Initialize(DirectXCommon* dxCommon);

	// シングルトンインスタンスの取得
	static ModelManager* GetInstance();

	// モデルファイルの読み込み
	void LoadModel(const std::string& filePath);

	// モデルの検索
	Model* FindModel(const std::string& filePath);

	// 終了
	void Finalize();
private:
	static ModelManager* instance;

	// コンストラクタ
	ModelManager() = default;
	// デストラクタ
	~ModelManager() = default;
	// コピーコンストラクタ
	ModelManager(ModelManager* modelManager) = delete;
	// コピー代入演算子
	ModelManager& operator=(const ModelManager& modelManager) = delete;

	// モデルデータ
	std::map<std::string, std::unique_ptr<Model>> models;
	// モデル共通部
	ModelCommon* modelCommon = nullptr;
};

