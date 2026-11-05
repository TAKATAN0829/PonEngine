#pragma once

#include <string>

// 履歴の選択とCSV出力の状態を所有する。計測データはFrameProfilerから借用する。
class PerformancePanel {
public:

	//=============================================================================================//
	// 描画処理

	// 表示メニューの開閉状態を受け取り、閉じたら呼出元にも反映する。
	void Draw(bool& isVisible);

private:

	//=============================================================================================//
	// 履歴の表示と出力結果

	bool shouldInspectPeak_ = false; // 最新フレームの代わりに最大負荷の記録を表示する。

	int selectedFrame_ = 0; // 記録停止中に確認する履歴の添字。

	std::string exportStatus_; // CSV出力結果だけを保持し、計測データは複製しない。
};
