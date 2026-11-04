#pragma once

#include "runtime/WorldScene.h"

// 共通のカメラ・描画・動作設定を、GameSceneと別の配置でも使えることを示す最小の場面。
class TitleScene : public WorldScene {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize(const SceneContext& context) override;

	//=============================================================================================//
	// 更新処理

	// 再生中のEnterでゲームへ進む。編集・Pause中は遷移しない。
	void Update(Input* input) override;

	//=============================================================================================//
	// Getter / 場面情報

	const char* GetName() const override { return "TitleScene"; }

	// Draw/Finalizeは共通実装を継承し、不要な空実装は追加しない。
};
