#include "ModelCommon.h"

// C++
#include <cassert>

//=================================================================================================//
// 初期化処理

void ModelCommon::Initialize(DirectXCommon* dxCommon) {

	assert(dxCommon);

	dxCommon_ = dxCommon;
}
