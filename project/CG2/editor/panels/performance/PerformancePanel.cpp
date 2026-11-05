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
		ImGui::Text("Frame #%llu: %.2f ms", static_cast<unsigned long long>(sample.number), sample.frameMilliseconds);

		float framesPerSecond = 0;

		if (sample.frameMilliseconds > 0) {
			framesPerSecond = 1000.0f / sample.frameMilliseconds;
		}

		ImGui::Text("FPS: %.1f   DrawCall: %llu   UI: %llu", framesPerSecond,
			static_cast<unsigned long long>(sample.drawCalls), static_cast<unsigned long long>(sample.uiDrawCalls));

		ImGui::Text("Triangles: %llu   Instances: %llu", static_cast<unsigned long long>(sample.triangles), static_cast<unsigned long long>(sample.instances));

		if (sample.isGpuTimingValid) {
			ImGui::Text("GPU execution: %.3f ms", sample.gpuMilliseconds);
		} else {
			ImGui::TextDisabled("GPU execution: N/A（未対応または計測結果未取得）");
		}

		if (ImGui::BeginTable("CpuBreakdown", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
			ImGui::TableSetupColumn("CPU区間（子区間は親に含む）");

			ImGui::TableSetupColumn("ms");

			ImGui::TableHeadersRow();

			for (size_t sectionIndex = 0; sectionIndex < static_cast<size_t>(FrameProfiler::Section::Count); ++sectionIndex) {
				ImGui::TableNextRow();

				ImGui::TableNextColumn();

				ImGui::TextUnformatted(FrameProfiler::GetSectionName(static_cast<FrameProfiler::Section>(sectionIndex)));

				ImGui::TableNextColumn();

				ImGui::Text("%.3f", sample.milliseconds[sectionIndex]);
			}

			ImGui::EndTable();
		}
	}

	void DrawPerformanceHistory(int& selectedFrame_) {
		// 固定長の履歴からフレーム時間の推移を描く。
		std::array<float, FrameProfiler::kHistorySize> durations{};

		float average = 0;
		float maximum = 0;

		for (size_t sampleIndex = 0; sampleIndex < FrameProfiler::GetCount(); ++sampleIndex) {
			durations[sampleIndex] = FrameProfiler::GetSample(sampleIndex).frameMilliseconds;

			average += durations[sampleIndex];

			maximum = (std::max)(maximum, durations[sampleIndex]);
		}

		if (FrameProfiler::GetCount() > 0) {
			average /= static_cast<float>(FrameProfiler::GetCount());
		}

		ImGui::Text("History: %zu / 600 frames   Avg %.2f ms   Max %.2f ms", FrameProfiler::GetCount(), average, maximum);

		ImGui::PlotLines("##FrameHistory", durations.data(), static_cast<int>(FrameProfiler::GetCount()), 0, "Frame time (ms) / click to inspect", 0, (std::max)(33.33f, maximum * 1.1f), {0, 95});

		if (ImGui::IsItemHovered()) {
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
				const float fraction = (ImGui::GetIO().MousePos.x - ImGui::GetItemRectMin().x) / ImGui::GetItemRectSize().x;

				selectedFrame_ = (std::clamp)(static_cast<int>(fraction * static_cast<float>(FrameProfiler::GetCount())), 0, static_cast<int>(FrameProfiler::GetCount()) - 1);

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

	ImGui::SetNextWindowSize({590, 650}, ImGuiCond_FirstUseEver);

	if (!ImGui::Begin("Performance", &isVisible)) {
		ImGui::End();

		return;
	}

	bool isPaused = FrameProfiler::IsPaused();

	if (ImGui::Checkbox("記録を一時停止（ゲームは継続）", &isPaused)) {
		FrameProfiler::SetPaused(isPaused);

		selectedFrame_ = (std::max)(0, static_cast<int>(FrameProfiler::GetCount()) - 1);
	}

	if (FrameProfiler::GetCount() == 0) {
		ImGui::TextUnformatted("計測開始待ち");

		ImGui::End();

		return;
	}

	DrawPerformanceHistory(selectedFrame_);

	ImGui::Checkbox("最も重かったフレームを表示", &shouldInspectPeak_);

	if (FrameProfiler::IsPaused()) {
		ImGui::SliderInt("履歴フレーム", &selectedFrame_, 0, static_cast<int>(FrameProfiler::GetCount()) - 1);
	}

	const FrameProfiler::Sample* sample = &FrameProfiler::GetLatest();

	if (FrameProfiler::IsPaused()) {
		sample = &FrameProfiler::GetSample(static_cast<size_t>(selectedFrame_));
	}

	if (shouldInspectPeak_) {
		sample = &FrameProfiler::GetPeak();
	}

	DrawPerformanceDetails(*sample);

	ImGui::TextWrapped("DrawCallはモデル・Sprite・Particle・Assetプレビューの発行回数です。UIは別集計。Trianglesはインスタンス数を含む送信量です。");

	ImGui::TextWrapped("表はCPU時間、GPU executionは同じフレームのGPUタイムスタンプ計測です。GPU時間にはScene・UI・プレビュー等の命令を含み、Present/VSyncやFPS制限のCPU待機は含みません。CPUとGPUは並行動作するため足し合わせないでください。GPU時間だけではGPU内のどの描画が重いかは断定できません。");

	if (ImGui::Button("CSV出力")) {
		if (FrameProfiler::ExportCsv("performance-capture.csv")) {
			exportStatus_ = "作業ディレクトリのperformance-capture.csvに保存しました。";
		} else {
			exportStatus_ = "CSVを書き込めませんでした。";
		}
	}

	ImGui::SameLine();

	if (ImGui::Button("最大フレームをリセット")) {
		FrameProfiler::ResetPeak();

		shouldInspectPeak_ = false;
	}

	ImGui::TextWrapped("%s", exportStatus_.c_str());

	ImGui::End();

#else
	(void)isVisible;

#endif
}
