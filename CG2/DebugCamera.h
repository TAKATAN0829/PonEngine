#pragma once
#include "MT3.h"

class DebugCamera
{
public:
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// 更新
	/// </summary>
	void Update();

private:
	// X,Y,Z軸回りのローカル回転角
	Vector3 rotation_ = { 0.f,0.f,0.f };
	// ローカル座標
	Vector3 translation_ = { 0.f,0.f,-50.f };
	// ビュー行列

	// 射影行列

};

