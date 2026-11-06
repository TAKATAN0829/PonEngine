#include "FrameProfiler.h"
#include <cmath>
#include <fstream>

//=============================================================================================//
// 型 / 計測区間・一フレームの記録

FrameProfiler::Scope::Scope(Section section) : section_(section), started_(Clock::now()) {
	// 計測先と開始時刻は初期化リストで記録する。
}

FrameProfiler::Scope::~Scope() {
	// 同じ区間を複数回通った場合も、経過時間を加算する。
	current_.milliseconds[static_cast<size_t>(section_)] += std::chrono::duration<float, std::milli>(Clock::now() - started_).count();
}

//=============================================================================================//
// 初期化・更新処理 / フレーム境界

void FrameProfiler::Initialize() {
	// 計測中・最新・ピークの記録を消す。
	current_ = {};

	latest_ = {};

	peak_ = {};

	// リングバッファに残った履歴を消す。
	history_ = {};

	// 記録件数と次の書込位置を戻す。
	count_ = 0;

	cursor_ = 0;

	// フレーム番号を最初から数え直す。
	number_ = 0;

	// 記録を再開した状態にする。
	isPaused_ = false;
}

void FrameProfiler::BeginFrame() {
	// 前フレームの時間と描画回数を持ち越さない。
	current_ = {};

	// 今回のフレームに通し番号を付ける。
	current_.number = ++number_;

	// フレーム全体の計測を開始する。
	started_ = Clock::now();
}

void FrameProfiler::EndFrame() {
	// 開始時刻からの経過時間をミリ秒で記録する。
	current_.frameMilliseconds = std::chrono::duration<float, std::milli>(Clock::now() - started_).count();

	// 表示を停止中は履歴とピークを更新しない。
	if (isPaused_) {
		return;
	}

	// 計測が完了したフレームを公開する。
	latest_ = current_;

	// 履歴の巡回で古いフレームが消えても参照できるよう、最大時間のサンプルを別に保管する。
	if (latest_.frameMilliseconds > peak_.frameMilliseconds) {
		peak_ = latest_;
	}

	// 現在の書込位置へ保存する。
	history_[cursor_] = latest_;

	// 末尾まで書いたら先頭へ戻る。
	cursor_ = (cursor_ + 1) % kHistorySize;

	// 記録件数はバッファの容量までに制限する。
	if (count_ < kHistorySize) {
		++count_;
	}
}

//=============================================================================================//
// Getter / 履歴・区間名

const FrameProfiler::Sample& FrameProfiler::GetLatest() {
	// 最後に記録したフレームを返す。
	return latest_;
}

const FrameProfiler::Sample& FrameProfiler::GetPeak() {
	// 計測時間が最大だったフレームを返す。
	return peak_;
}

size_t FrameProfiler::GetCount() {
	// 現在保持している履歴の件数を返す。
	return count_;
}

const FrameProfiler::Sample& FrameProfiler::GetSample(size_t index) {
	// 次の書込位置と件数から、最も古い記録を求める。
	const size_t first = (cursor_ + kHistorySize - count_) % kHistorySize;

	// 古い順の添字をリング内の位置へ変換する。
	return history_[(first + index) % kHistorySize];
}

bool FrameProfiler::IsPaused() {
	// ゲームではなく、計測履歴の停止状態を返す。
	return isPaused_;
}

const char* FrameProfiler::GetSectionName(Section section) {
	// Sectionの定義順に表示名を並べる。
	static constexpr const char* names[] = {
		"Editor UI / history (total)",
		"History / shortcuts",
		"Project / Console",
		"Hierarchy", 
		"Inspector", 
		"SceneView UI / picking",
		"Model load (incl. textures)",
		"Texture load", 
		"Scene update (total)",
		"Motion components",
		"Physics / gravity / response",
		"Collision detection",
		"Models (incl. animation)",
		"CPU animation", 
		"Particles",
		"Asset previews", 
		"Scene draw recording", 
		"ImGui draw recording",
		"Submit / reset (total)",
		"Present / VSync", 
		"GPU fence wait",
		"FPS limiter"
	};

	// 計測区間に対応する名前を返す。
	return names[static_cast<size_t>(section)];
}

//=============================================================================================//
// Setter / 描画カウンター・記録制御

void FrameProfiler::SetPaused(bool isPaused) {
	// 次のフレーム終了時から記録の可否を切り替える。
	isPaused_ = isPaused;
}

void FrameProfiler::ResetPeak() {
	// 最新の履歴を残したまま、ピークだけ消す。
	peak_ = {};
}

void FrameProfiler::RecordDraw(uint32_t indices, uint32_t instances) {
	// 描画命令はインスタンス数に関係なく一回と数える。
	++current_.drawCalls;

	// 三角形数はインスタンス分も含めて数える。
	current_.triangles += static_cast<uint64_t>(indices / 3) * instances;

	// 描画したインスタンスの総数を加算する。
	current_.instances += instances;
}

void FrameProfiler::RecordUiDraw() {
	// ImGuiの描画回数は通常の描画と分けて数える。
	++current_.uiDrawCalls;
}

void FrameProfiler::SetGpuMilliseconds(float milliseconds) {
	// 有効な値を確認できるまでは未取得として扱う。
	current_.isGpuTimingValid = false;

	current_.gpuMilliseconds = 0;

	// NaN・無限大・負の時間を履歴へ入れない。
	if (!std::isfinite(milliseconds) || milliseconds < 0) {
		return;
	}

	// 0msも有効な計測値として保存する。
	current_.gpuMilliseconds = milliseconds;

	current_.isGpuTimingValid = true;
}

bool FrameProfiler::ExportCsv(const std::string& path) {
	// 指定されたファイルへ上書きで出力する。
	std::ofstream output(path, std::ios::trunc);

	// フレーム全体と描画負荷の列名を書く。
	output << "frame,frame_ms,draw_calls,ui_draw_calls,triangles,instances,gpu_valid,gpu_ms";

	// 計測区間ごとの列名を追加する。
	for (size_t sectionIndex = 0; sectionIndex < static_cast<size_t>(Section::Count); ++sectionIndex) {
		output << ',' << GetSectionName(static_cast<Section>(sectionIndex));
	}

	output << '\n';

	// 履歴を古いフレームから一行ずつ書く。
	for (size_t sampleIndex = 0; sampleIndex < count_; ++sampleIndex) {
		const Sample& sample = GetSample(sampleIndex);

		output << sample.number << ',' << sample.frameMilliseconds << ',' << sample.drawCalls << ',' << sample.uiDrawCalls << ',' << sample.triangles << ',' << sample.instances;

		output << ',' << sample.isGpuTimingValid << ',';

		// GPU時間を取得できなかった場合は空欄にする。
		if (sample.isGpuTimingValid) {
			output << sample.gpuMilliseconds;
		}

		// フレーム内の各区間の時間を追加する。
		for (float duration : sample.milliseconds) {
			output << ',' << duration;
		}

		output << '\n';
	}

	// ファイルを閉じる際の書込失敗も確認する。
	output.close();

	return static_cast<bool>(output);
}
