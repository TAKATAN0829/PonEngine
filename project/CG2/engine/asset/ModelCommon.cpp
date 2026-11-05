#include "ModelCommon.h"

// C++
#include <cassert>

//=============================================================================================//
// 初期化処理


void ModelCommon::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
	// モデル描画で借用する共通描画機能とSRV管理を保持する。

	assert(dxCommon);

	assert(srvManager);

	dxCommon_ = dxCommon;

	srvManager_ = srvManager;
}

//=============================================================================================//
// Getter

DirectXCommon* ModelCommon::GetDxCommon() const {
	// 借用している共通描画機能を取得する。
	return dxCommon_;
}

SrvManager* ModelCommon::GetSrvManager() const {
	// 借用している共有SRV管理機能を取得する。
	return srvManager_;
}
