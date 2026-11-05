#pragma once

#include "EngineStructs.h"
#include <cmath>

enum class BodyMode { Static, Dynamic, Kinematic };

// 並進運動用のRigidbody設定。実行中の速度はWorldPhysicsが別に所有する。
struct RigidBodySettings {
	//=============================================================================================//
	// 存在・動作方式

	bool isPresent = false; // 有効な親RigidbodyもないColliderは固定物として扱う。
	bool isEnabled = true;

	BodyMode mode = BodyMode::Dynamic;

	bool isGravityEnabled = true;

	//=============================================================================================//
	// 質量・接触特性・初速

	float mass = 1; // kg。Dynamic同士の押し戻し・速度変化を質量で配分する。
	float gravityScale = 1;
	float restitution = 0; // 0は跳ね返らず、1は弾性衝突。
	float friction = 0.6f;
	float linearDamping = 0;

	Vector3 initialVelocity{}; // ワールド単位/秒。Play開始時だけ適用する。
};

bool AreBodySettingsValid(const RigidBodySettings& settings);

bool AreBodySettingsEqual(const RigidBodySettings& first, const RigidBodySettings& second);
