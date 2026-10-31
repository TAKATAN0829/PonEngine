#pragma once
#include "IEditableScene.h"
#include <unordered_map>

// ゲーム更新時だけTransformを動かす。描画はGameSceneへ任せ、GPU資源は所有しない。
class SceneMotion {
public:
	//=============================================================================================//
	// 初期化・終了処理

	// 再生開始・停止で時計を破棄する。編集状態の復元はEditorSessionが担当する。
	void Initialize();
	void Finalize();

	//=============================================================================================//
	// 更新処理

	// Pause中は呼ばない。Activeが無効な親を持つ対象も時計を進めない。
	void Update(IEditableScene& scene, float deltaSeconds);

private:
	//=============================================================================================//
	// 型 / オブジェクトごとの往復移動状態

	struct MovementState {
		Vector3 origin{}; // 最初に動き始めたフレームのローカル位置。
		double elapsed = 0; // 大きな累積誤差を避けるためdoubleで保持する。
		bool initialized = false;
	};

	//=============================================================================================//
	// メンバ関数 / 動作の種類別更新

	static void UpdateRotation(const SceneMotionSettings& settings, float deltaSeconds, TransformData& transform);
	static void UpdateMovement(const SceneMotionSettings& settings, float deltaSeconds, TransformData& transform, MovementState& state);

private:
	//=============================================================================================//
	// メンバ変数 / 保存対象外の実行状態

	std::unordered_map<SceneObjectId, MovementState> movementStates_; // IDだけで対応付け、借用参照を保持しない。
};
