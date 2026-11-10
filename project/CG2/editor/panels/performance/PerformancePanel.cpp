#include "PerformancePanel.h"
#include <algorithm>
#include "FrameProfiler.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

#ifdef USE_IMGUI

//=============================================================================================//
// 実装内補助定義

namespace {
	// 一覧の時間はCPU区間。GPU待機時間をGPU描画時間として解釈しない。
	void DrawPerformanceDetails(const FrameProfiler::Sample& sample) {
		// 選択フレームの計測区間を比較し、負荷の内訳を表示する。
		// 「Frame #%llu: %.2f ms」テキストを表示。
		ImGui::Text("Frame #%llu: %.2f ms", static_cast<unsigned long long>(sample.number), sample.frameMilliseconds);

		float framesPerSecond = 0;

		// 1秒をフレーム時間で割る。計測時間が0なら除算しない。
		if (sample.frameMilliseconds > 0) {
			framesPerSecond = 1000.0f / sample.frameMilliseconds;
		}

		// テキストを表示。
		ImGui::Text("FPS: %.1f   DrawCall: %llu   UI: %llu", framesPerSecond,
			static_cast<unsigned long long>(sample.drawCalls), static_cast<unsigned long long>(sample.uiDrawCalls));

		// テキストを表示。
		ImGui::Text("Triangles: %llu   Instances: %llu", static_cast<unsigned long long>(sample.triangles), static_cast<unsigned long long>(sample.instances));

		// GPUのタイムスタンプがそろったサンプルだけ、GPU実行時間を表示する。
		if (sample.isGpuTimingValid) {
			// 「GPU execution: %.3f ms」テキストを表示。
			ImGui::Text("GPU execution: %.3f ms", sample.gpuMilliseconds);
		} else {
			// 補助テキストを表示。
			ImGui::TextDisabled("GPU execution: N/A（未対応または計測結果未取得）");
		}

		// 「CpuBreakdown」表を開始。
		if (ImGui::BeginTable("CpuBreakdown", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
			// 「CPU区間（子区間は親に含む）」表の列を設定。
			ImGui::TableSetupColumn("CPU区間（子区間は親に含む）");
			// 「ms」表の列を設定。
			ImGui::TableSetupColumn("ms");
			// 表の見出しを表示。
			ImGui::TableHeadersRow();

			for (size_t sectionIndex = 0; sectionIndex < static_cast<size_t>(FrameProfiler::Section::Count); ++sectionIndex) {
				// 表の次行へ移動。
				ImGui::TableNextRow();
				// 表の次列へ移動。
				ImGui::TableNextColumn();
				// テキストを表示。
				ImGui::TextUnformatted(FrameProfiler::GetSectionName(static_cast<FrameProfiler::Section>(sectionIndex)));
				// 表の次列へ移動。
				ImGui::TableNextColumn();
				// テキストを表示。
				ImGui::Text("%.3f", sample.milliseconds[sectionIndex]);
			}

			// 表を終了。
			ImGui::EndTable();
		}
	}

	void DrawPerformanceHistory(int& selectedFrame_) {
		// 固定長の履歴からフレーム時間の推移を描く。
		std::array<float, FrameProfiler::kHistorySize> durations{};

		float average = 0;
		float maximum = 0;

		// 履歴をグラフ用の配列に写し、同時に平均と縦軸の上限に使う値を求める。
		for (size_t sampleIndex = 0; sampleIndex < FrameProfiler::GetCount(); ++sampleIndex) {
			durations[sampleIndex] = FrameProfiler::GetSample(sampleIndex).frameMilliseconds;

			average += durations[sampleIndex];

			maximum = (std::max)(maximum, durations[sampleIndex]);
		}

		if (FrameProfiler::GetCount() > 0) {
			average /= static_cast<float>(FrameProfiler::GetCount());
		}

		// テキストを表示。
		ImGui::Text("History: %zu / 600 frames   Avg %.2f ms   Max %.2f ms", FrameProfiler::GetCount(), average, maximum);
		// 折れ線グラフを表示。
		ImGui::PlotLines("##FrameHistory", durations.data(), static_cast<int>(FrameProfiler::GetCount()), 0, "Frame time (ms) / click to inspect", 0, (std::max)(33.33f, maximum * 1.1f), {0, 95});
		// ホバー状態を確認。
		if (ImGui::IsItemHovered()) {
			// マウスクリックを確認。
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
				// グラフ上の横位置を0〜1の割合に変換して、対応するフレームを選ぶ。
				// 入力状態を取得。
				const float fraction = (ImGui::GetIO().MousePos.x - ImGui::GetItemRectMin().x) / ImGui::GetItemRectSize().x;

				selectedFrame_ = (std::clamp)(static_cast<int>(fraction * static_cast<float>(FrameProfiler::GetCount())), 0, static_cast<int>(FrameProfiler::GetCount()) - 1);

				// 選んだフレームが履歴の更新でずれないよう、計測記録を止める。
				FrameProfiler::SetPaused(true);
			}
		}
	}
}

#endif

//=============================================================================================//
// 描画処理


// 画面を開いていなくても固定長で記録を続け、発生後に履歴を調べられるようにする。
void PerformancePanel::Draw(bool& isVisible) {
	// 記録の停止・ピーク・選択フレームを操作し、計測履歴を表示する。
#ifdef USE_IMGUI
	if (!isVisible) {
		return;
	}

	// 次のウィンドウサイズを設定。
	ImGui::SetNextWindowSize({590, 650}, ImGuiCond_FirstUseEver);
	// 「Performance」ウィンドウを開始。
	if (!ImGui::Begin("Performance", &isVisible)) {
		// ウィンドウを終了。
		ImGui::End();

		return;
	}

	bool isPaused = FrameProfiler::IsPaused();

	// 「記録を一時停止（ゲームは継続）」チェック状態を編集。
	if (ImGui::Checkbox("記録を一時停止（ゲームは継続）", &isPaused)) {
		FrameProfiler::SetPaused(isPaused);

		selectedFrame_ = (std::max)(0, static_cast<int>(FrameProfiler::GetCount()) - 1);
	}

	if (FrameProfiler::GetCount() == 0) {
		// 「計測開始待ち」テキストを表示。
		ImGui::TextUnformatted("計測開始待ち");
		// ウィンドウを終了。
		ImGui::End();

		return;
	}

	DrawPerformanceHistory(selectedFrame_);

	// 「最も重かったフレームを表示」チェック状態を編集。
	ImGui::Checkbox("最も重かったフレームを表示", &shouldInspectPeak_);

	if (FrameProfiler::IsPaused()) {
		// 「履歴フレーム」スライダーで整数編集。
		ImGui::SliderInt("履歴フレーム", &selectedFrame_, 0, static_cast<int>(FrameProfiler::GetCount()) - 1);
	}

	// 通常は最新値を表示し、停止中は選んだ履歴、ピーク表示中は最大フレームを使う。
	const FrameProfiler::Sample* sample = &FrameProfiler::GetLatest();

	if (FrameProfiler::IsPaused()) {
		sample = &FrameProfiler::GetSample(static_cast<size_t>(selectedFrame_));
	}

	if (shouldInspectPeak_) {
		sample = &FrameProfiler::GetPeak();
	}

	DrawPerformanceDetails(*sample);

	// 折り返しテキストを表示。
	ImGui::TextWrapped("DrawCallはモデル・Sprite・Particle・Assetプレビューの発行回数です。UIは別集計。Trianglesはインスタンス数を含む送信量です。");
	// 折り返しテキストを表示。
	ImGui::TextWrapped("表はCPU時間、GPU executionは同じフレームのGPUタイムスタンプ計測です。GPU時間にはScene・UI・プレビュー等の命令を含み、Present/VSyncやFPS制限のCPU待機は含みません。CPUとGPUは並行動作するため足し合わせないでください。GPU時間だけではGPU内のどの描画が重いかは断定できません。");
	// 「CSV出力」ボタン押下を受け取る。
	if (ImGui::Button("CSV出力")) {
		if (FrameProfiler::ExportCsv("performance-capture.csv")) {
			exportStatus_ = "作業ディレクトリのperformance-capture.csvに保存しました。";
		} else {
			exportStatus_ = "CSVを書き込めませんでした。";
		}
	}

	// 横並びに配置。
	ImGui::SameLine();
	// 「最大フレームをリセット」ボタン押下を受け取る。
	if (ImGui::Button("最大フレームをリセット")) {
		FrameProfiler::ResetPeak();

		shouldInspectPeak_ = false;
	}

	// 折り返しテキストを表示。
	ImGui::TextWrapped("%s", exportStatus_.c_str());
	// ウィンドウを終了。
	ImGui::End();

#else
	(void)isVisible;

#endif
}
