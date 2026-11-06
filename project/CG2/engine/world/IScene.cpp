#include "IScene.h"

//=============================================================================================//
// Getter / 場面情報・遷移要求

const char* IScene::GetName() const {
	// 場面固有の名前を持たない場合の既定名を返す。
	return "Scene";
}

// 起動時に読込成功した配置ファイル。Editorが最初の保存先を決めるための情報。
std::string IScene::GetInitialAssetPath() const {
	// 起動時の読込ファイルを持たない既定実装として、空のパスを返す。
	return {};
}

// 更新中は要求だけを取り出す。破棄と初期化はSceneManagerがフレーム境界で行う。
std::unique_ptr<IScene> IScene::TakeNextScene() {
	// 遷移管理側へ所有権を渡すと予約は空になり、次の遷移を受け付けられる。
	return std::move(nextScene_);
}

// 編集機能に対応するシーンだけが窓口を返す。Editorのパネル型には依存しない。
IEditableWorld* IScene::GetEditableWorld() {
	// 編集機能を持たない既定実装として、参照を返さない。
	return nullptr;
}

// 再生の開始・停止時に、シーン固有の実行データを初期化するための通知。
void IScene::OnPlayStateChanged(bool isPlaying) {
	// 再生状態の通知が不要な場面では、既定で処理を行わない。
	(void)isPlaying;
}

// 通常のシーンは再生中だけ更新する。編集時も描画更新が必要なら派生側で実装する。
void IScene::UpdateEditorFrame(Input* input, bool shouldSimulate) {
	// 編集・一時停止中の表示更新と、再生中のゲーム更新を分ける。
	if (shouldSimulate) {
		Update(input);
	}
}

//=============================================================================================//
// シーン遷移

// 未初期化の次シーンを渡す。同じフレームでは最初の有効な要求を採用する。
void IScene::RequestSceneChange(std::unique_ptr<IScene> nextScene) {
	// 同じフレームでは最初の遷移要求だけを保持する。
	if (nextScene_ == nullptr) {
		nextScene_ = std::move(nextScene);
	}
}
