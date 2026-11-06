#pragma once

#include "EngineStructs.h"
#include <cmath>

enum class BodyMode { Static, Dynamic, Kinematic };

// 並進運動用のRigidbody設定。実行中の速度はWorldPhysicsが別に所有する。
struct RigidBodySettings {
	RigidBodySettings();

	//=============================================================================================//
	// 存在・動作方式

	bool isPresent; // 有効な親RigidbodyもないColliderは固定物として扱う。
	bool isEnabled;

	BodyMode mode;

	bool isGravityEnabled;

	//=============================================================================================//
	// 質量・接触特性・初速

	float mass; // kg。Dynamic同士の押し戻し・速度変化を質量で配分する。
	float gravityScale;
	float restitution; // 0は跳ね返らず、1は弾性衝突。
	float friction;
	float linearDamping;

	Vector3 initialVelocity; // ワールド単位/秒。Play開始時だけ適用する。
};

bool AreBodySettingsValid(const RigidBodySettings& settings);

bool AreBodySettingsEqual(const RigidBodySettings& first, const RigidBodySettings& second);
