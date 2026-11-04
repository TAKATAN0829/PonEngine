#pragma once

#include "systems/WorldCollision.h"
#include <unordered_map>

// 並進の重力・接触応答。Colliderの形状計算を共有し、GPUやログには依存しない。
class WorldPhysics {
public:

	//=============================================================================================//
	// 初期化・終了処理

	void Initialize(IWorldObjects& scene); // Play開始時の初速とKinematicの位置を取得する。

	void Finalize(); // Stop時に速度を消す。Transformの復元はEditorSessionが担当する。

	//=============================================================================================//
	// 更新処理

	void Update(IWorldObjects& scene, float deltaSeconds);

	//=============================================================================================//
	// Getter / 実行中の速度

	Vector3 GetVelocity(WorldObjectId identifier) const;

	const std::string& GetError() const { return error_; } // 再生中に不正な設定へ変更された場合の理由。

private:

	//=============================================================================================//
	// 型 / 保存対象外の運動状態・接触候補

	struct BodyState {
		Vector3 velocity{}; // ワールド単位/秒。
		Vector3 previousPosition{};
		Vector3 frameStart{}; // Kinematicの移動をサブステップへ分配する。
		Vector3 frameTarget{};

		BodyMode mode = BodyMode::Static;

		float inverseMass = 0;

		bool active = false;
	};

	struct ShapeEntry {
		WorldCollider shape;

		WorldObjectId owner = 0; // 親Rigidbodyの子Colliderも同じ質量として扱う。

		bool trigger = false;
	};

	//=============================================================================================//
	// 速度・移動・接触解決

	void RefreshBodies(IWorldObjects& scene, float deltaSeconds);

	void Integrate(IWorldObjects& scene, float deltaSeconds);

	void AdvanceKinematics(IWorldObjects& scene, float fraction);

	void Solve(IWorldObjects& scene);

	std::vector<ShapeEntry> CollectShapes(IWorldObjects& scene) const;

	bool ResolvePair(IWorldObjects& scene, const ShapeEntry& first, const ShapeEntry& second);

	void ApplyImpulse(WorldObjectId first, WorldObjectId second, const Vector3& normal, float restitution, float friction);

	void MoveBody(IWorldObjects& scene, WorldObjectId identifier, const Vector3& delta);

	float GetInverseMass(WorldObjectId identifier) const;

	static bool CompareShapes(const ShapeEntry& first, const ShapeEntry& second);

private:

	//=============================================================================================//
	// Rigidbody状態

	std::unordered_map<WorldObjectId, BodyState> bodies_; // IDだけを所有し、借用参照を保持しない。

	std::string error_; // 不正な設定の間は物理だけを止め、爆発的な押し戻しを防ぐ。
};
