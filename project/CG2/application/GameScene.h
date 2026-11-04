#pragma once

#include "runtime/WorldScene.h"
#include "Sprite.h"
#include "SpriteCommon.h"

// ゲーム固有の初期配置・保存データ・HUDと、タイトルへ戻る操作を定義する。
class GameScene : public WorldScene {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize(const SceneContext& context) override;

	//=============================================================================================//
	// 更新処理

	void Update(Input* input) override;

	//=============================================================================================//
	// 描画処理

	void Draw() override;

	void DrawEditorTools(EditorToolVisibility& visibility) override;

	//=============================================================================================//
	// 終了処理

	void Finalize() override;

	//=============================================================================================//
	// Getter / 場面情報

	const char* GetName() const override { return "GameScene"; }

	std::string GetInitialAssetPath() const override { return initialAssetPath_; }

	//=============================================================================================//
	// Setter / HUD表示サイズ

	void SetEditorViewport(uint32_t width, uint32_t height) override;

private:

	//=============================================================================================//
	// 設定パネル描画

	// サンプルHUDの設定は、このHUDを持つゲームシーンだけで扱う。
	void DrawWorldSettings(bool* open);

	//=============================================================================================//
	// 起動時の配置ファイル

	std::string initialAssetPath_; // Editorの初回接続用。編集中の保存先はSceneDocumentが所有する。

	//=============================================================================================//
	// ゲームHUD

	SpriteCommon* spriteCommon_ = nullptr; // Engineが所有し、このシーンより長く生存する。

	std::unique_ptr<Sprite> sprite_; // サンプル画像の描画リソースを所有する。

	Vector2 spritePosition_ = {100, 100};

	float spriteRotation_ = 0;

	Vector2 spriteSize_ = {256, 256};
};
