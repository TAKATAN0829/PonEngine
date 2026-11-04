#pragma once

// 自作
#include "Input.h"
#include <memory>
#include <string>
#include <utility>

class IEditableWorld;
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

	//=============================================================================================//
	// Getter / 場面情報・遷移要求

	virtual const char* GetName() const { return "Scene"; }

	// 起動時に読込成功した配置ファイル。Editorが最初の保存先を決めるための情報。
	virtual std::string GetInitialAssetPath() const { return {}; }

	// 更新中は要求だけを取り出す。破棄と初期化はSceneManagerがフレーム境界で行う。
	std::unique_ptr<IScene> TakeNextScene() { return std::move(nextScene_); }

	// 編集機能に対応するシーンだけが窓口を返す。Editorのパネル型には依存しない。
	virtual IEditableWorld* GetEditableWorld() { return nullptr; }

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

protected:

	//=============================================================================================//
	// シーン遷移

	// 未初期化の次シーンを渡す。同じフレームでは最初の有効な要求を採用する。
	void RequestSceneChange(std::unique_ptr<IScene> nextScene) {
		if (nextScene_ == nullptr) {
			nextScene_ = std::move(nextScene);
		}
	}

private:

	//=============================================================================================//
	// 遷移待ちの場面

	std::unique_ptr<IScene> nextScene_; // 初期化前なので、未使用の要求は通常の破棄で解放する。
};
