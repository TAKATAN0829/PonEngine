#include "ModelCommon.h"

// C++
#include <cassert>

//=================================================================================================//
// 初期化処理

void ModelCommon::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {

	assert(dxCommon);
	assert(srvManager);

	dxCommon_ = dxCommon;

	srvManager_ = srvManager;
}
