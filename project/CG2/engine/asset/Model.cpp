#include "Model.h"

void Model::Initialize(ModelCommon* modelCommon) {
	modelCommon_ = modelCommon;
	// モデル読み込み

	// 頂点データの初期化

	// マテリアルの初期化

	// テクスチャ読み込み

	// テクスチャ番号を取得して、メンバ変数に書き込む

}

void Model::Update() {

}

void Model::Draw() {
	// VertexBufferViewを設定

	// マテリアルCBufferの場所を特定

	// SRVのDescripterTableの先頭を設定

	// DrawCall
}