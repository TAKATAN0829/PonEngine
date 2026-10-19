#pragma once

class ModelCommon;

// 3Dモデル
class Model
{
public: // メンバ関数
	// 初期化
	void Initialize(ModelCommon* modelCommon);

	// 更新
	void Update();

	// 描画
	void Draw();
private:
	// ModelCommonのポインタ
	ModelCommon* modelCommon_;
	// Objファイルのデータ

	// VertexResource

	// VertexBufferResource

	// VertexBufferResourceにデータを書き込むためのポインタ

	// マテリアルリソース

	// マテリアルリソースにデータを書き込むためのポインタ
};

