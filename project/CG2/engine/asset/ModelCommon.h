#pragma once
#include "../dx12/DirectXCommon.h"


class ModelCommon
{
public:
	// 初期化
	void Initialize(DirectXCommon* dxCommon);

public:
	//=============================================================================================//
	// Getter
	
	DirectXCommon* GetDxCommon()const { return dxCommon_; }


private:
	DirectXCommon* dxCommon_;


};

