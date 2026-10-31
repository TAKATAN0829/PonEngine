#pragma once
#include "SceneCollision.h"
#include <unordered_map>

// 並進の重力・接触応答。Colliderの形状計算を共有し、GPUやログには依存しない。
class ScenePhysics {
public:
	//=============================================================================================//
	// 初期化・終了処理

	void Initialize(IEditableScene& scene); // Play開始時の初速とKinematicの位置を取得する。
	void Finalize(); // Stop時に速度を消す。Transformの復元はEditorSessionが担当する。

	//=============================================================================================//
	// 更新処理

	void Update(IEditableScene& scene, float deltaSeconds);

	//=============================================================================================//
	// Getter / 実行中の速度

	Vector3 GetVelocity(SceneObjectId identifier) const;
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
		SceneObjectId owner = 0; // 親Rigidbodyの子Colliderも同じ質量として扱う。
		bool trigger = false;
	};

	//=============================================================================================//
	// メンバ関数 / 速度・移動・接触解決

	void RefreshBodies(IEditableScene& scene, float deltaSeconds);
	void Integrate(IEditableScene& scene, float deltaSeconds);
	void AdvanceKinematics(IEditableScene& scene, float fraction);
	void Solve(IEditableScene& scene);
	std::vector<ShapeEntry> CollectShapes(IEditableScene& scene) const;
	bool ResolvePair(IEditableScene& scene, const ShapeEntry& first, const ShapeEntry& second);
	void ApplyImpulse(SceneObjectId first, SceneObjectId second, const Vector3& normal, float restitution, float friction);
	void MoveBody(IEditableScene& scene, SceneObjectId identifier, const Vector3& delta);
	float GetInverseMass(SceneObjectId identifier) const;
	static bool CompareShapes(const ShapeEntry& first, const ShapeEntry& second);

private:
	//=============================================================================================//
	// メンバ変数 / Rigidbody状態

	std::unordered_map<SceneObjectId, BodyState> bodies_; // IDだけを所有し、借用参照を保持しない。
	std::string error_; // 不正な設定の間は物理だけを止め、爆発的な押し戻しを防ぐ。
};
