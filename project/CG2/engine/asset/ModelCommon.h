#pragma once

class DirectXCommon;
class SrvManager;

class ModelCommon {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize (DirectXCommon* dxCommon, SrvManager* srvManager);

	//=============================================================================================//
	// Getter

	DirectXCommon* GetDxCommon () const { return dxCommon_; }

	SrvManager* GetSrvManager () const { return srvManager_; }

private:

	//=============================================================================================//
	// DirectXCommon

	DirectXCommon* dxCommon_ = nullptr;

	//=============================================================================================//
	// SrvManager

	SrvManager* srvManager_ = nullptr;
};
