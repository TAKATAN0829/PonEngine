#pragma once

// 自作
#include "Input.h"

class IEditableScene;
class Object3dCommon;
class SpriteCommon;
class ParticleRenderer;

//=================================================================================================//
// シーンに渡す描画機能

struct SceneContext {

	Object3dCommon* object3dCommon = nullptr;

	SpriteCommon* spriteCommon = nullptr;

	ParticleRenderer* particleRenderer = nullptr;
};

//=================================================================================================//
// シーン基底クラス

class IScene {
public:

	virtual ~IScene() = default;

	// 編集機能に対応するシーンだけが窓口を返す。Editorのパネル型には依存しない。
	virtual IEditableScene* GetEditableScene() { return nullptr; }
	// 再生の開始・停止時に、シーン固有の実行データを初期化するための通知。
	virtual void OnPlayStateChanged(bool playing) { (void)playing; }
	// 通常のシーンは再生中だけ更新する。編集時も描画更新が必要なら派生側で実装する。
	virtual void UpdateEditorFrame(Input* input, bool simulate) {
		if (simulate) {
			Update(input);
		}
	}

	//=============================================================================================//
	// 初期化処理

	virtual void Initialize(const SceneContext& context) = 0;

	//=============================================================================================//
	// 更新処理

	virtual void Update(Input* input) = 0;

	//=============================================================================================//
	// 描画処理

	virtual void Draw() = 0;

	//=============================================================================================//
	// 終了処理

	virtual void Finalize() = 0;
};
