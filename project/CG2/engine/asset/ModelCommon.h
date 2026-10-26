#pragma once

class DirectXCommon;

class ModelCommon {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize (DirectXCommon* dxCommon);

	//=============================================================================================//
	// Getter

	DirectXCommon* GetDxCommon () const { return dxCommon_; }

private:

	//=============================================================================================//
	// DirectXCommon

	DirectXCommon* dxCommon_ = nullptr;
};
