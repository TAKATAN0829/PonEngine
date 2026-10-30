#include "Editor.h"
#include "FrameProfiler.h"
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

#ifdef USE_IMGUI
namespace {
	// 一覧の時間はCPU区間。GPU待機時間をGPU描画時間として解釈しない。
	void DrawPerformanceDetails(const FrameProfiler::Sample& sample) {
		ImGui::Text("Frame #%llu: %.2f ms", static_cast<unsigned long long>(sample.number), sample.frameMilliseconds);
		float framesPerSecond = 0;
		if (sample.frameMilliseconds > 0) {
			framesPerSecond = 1000.0f / sample.frameMilliseconds;
		}
		ImGui::Text("FPS: %.1f   DrawCall: %llu   UI: %llu", framesPerSecond,
			static_cast<unsigned long long>(sample.drawCalls), static_cast<unsigned long long>(sample.uiDrawCalls));
		ImGui::Text("Triangles: %llu   Instances: %llu", static_cast<unsigned long long>(sample.triangles), static_cast<unsigned long long>(sample.instances));
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

	void DrawPerformanceHistory(int& selectedFrame) {
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
				selectedFrame = (std::clamp)(static_cast<int>(fraction * static_cast<float>(FrameProfiler::GetCount())), 0, static_cast<int>(FrameProfiler::GetCount()) - 1);
				FrameProfiler::SetPaused(true);
			}
		}
	}
}
#endif

// 画面を開いていなくても固定長で記録を続け、発生後に履歴を調べられるようにする。
void Editor::DrawPerformance() {
#ifdef USE_IMGUI
	if (!toolVisibility_.performance) {
		return;
	}
	ImGui::SetNextWindowSize({590, 650}, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Performance", &toolVisibility_.performance)) {
		ImGui::End();
		return;
	}
	static bool inspectPeak = false;
	static int selectedFrame = 0;
	static std::string exportStatus;
	bool paused = FrameProfiler::IsPaused();
	if (ImGui::Checkbox("記録を一時停止（ゲームは継続）", &paused)) {
		FrameProfiler::SetPaused(paused);
		selectedFrame = (std::max)(0, static_cast<int>(FrameProfiler::GetCount()) - 1);
	}
	if (FrameProfiler::GetCount() == 0) {
		ImGui::TextUnformatted("計測開始待ち");
		ImGui::End();
		return;
	}
	DrawPerformanceHistory(selectedFrame);
	ImGui::Checkbox("最も重かったフレームを表示", &inspectPeak);
	if (FrameProfiler::IsPaused()) {
		ImGui::SliderInt("履歴フレーム", &selectedFrame, 0, static_cast<int>(FrameProfiler::GetCount()) - 1);
	}
	const FrameProfiler::Sample* sample = &FrameProfiler::GetLatest();
	if (FrameProfiler::IsPaused()) {
		sample = &FrameProfiler::GetSample(static_cast<size_t>(selectedFrame));
	}
	if (inspectPeak) {
		sample = &FrameProfiler::GetPeak();
	}
	DrawPerformanceDetails(*sample);
	ImGui::TextWrapped("DrawCallはモデル・Sprite・Particle・Assetプレビューの発行回数です。UIは別集計。Trianglesはインスタンス数を含む送信量です。");
	ImGui::TextWrapped("時間はCPU計測です。PresentはVSync、GPU fence waitは完了待ち、FPS limiterは60fps制限の待機です。これらの待機だけでGPU負荷は断定できません。区間外の入力・描画先の準備等もFrame timeには含まれます。");
	if (ImGui::Button("CSV出力")) {
		if (FrameProfiler::ExportCsv("performance-capture.csv")) {
			exportStatus = "作業ディレクトリのperformance-capture.csvに保存しました。";
		} else {
			exportStatus = "CSVを書き込めませんでした。";
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("最大フレームをリセット")) {
		FrameProfiler::ResetPeak();
		inspectPeak = false;
	}
	ImGui::TextWrapped("%s", exportStatus.c_str());
	ImGui::End();
#endif
}

// 複数選択では共通項目を表示し、Transformは主選択との差分で編集する。
void Editor::DrawMultipleInspector(IEditableScene& scene) {
#ifdef USE_IMGUI
	if (!ImGui::Begin("Inspector")) {
		ImGui::End();
		return;
	}
	ImGui::Text("選択: %zu objects", selection_.GetIds().size());
	ImGui::TextWrapped("主選択の値を表示します。変更した項目だけを反映します。Transformはローカル差分を選択ルートへ、色とActiveは選択全体へ適用します。");
	EditableObjectView primary;
	for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
		if (scene.GetEditableObject(objectIndex).id == selection_.Get()) {
			primary = scene.GetEditableObject(objectIndex);
			break;
		}
	}
	if (primary.transform == nullptr) {
		ImGui::End();
		return;
	}
	if (primary.active == nullptr || primary.color == nullptr) {
		ImGui::End();
		return;
	}
	const TransformData original = *primary.transform;
	TransformData edited = original;
	bool transformChanged = ImGui::DragFloat3("Position (local)", &edited.translate.x, 0.05f);
	transformChanged |= ImGui::DragFloat3("Rotation (radians)", &edited.rotate.x, 0.01f);
	transformChanged |= ImGui::DragFloat3("Scale", &edited.scale.x, 0.01f);
	bool active = *primary.active;
	const bool activeChanged = ImGui::Checkbox("Active (all)", &active);
	Vector4 color = *primary.color;
	const bool colorChanged = ImGui::ColorEdit4("Color (all)", &color.x);
	if (transformChanged || activeChanged || colorChanged) {
		std::unordered_set<SceneObjectId> roots;
		for (SceneObjectId identifier : selection_.GetRoots(scene)) {
			roots.insert(identifier);
		}
		for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
			const EditableObjectView object = scene.GetEditableObject(objectIndex);
			if (!selection_.Contains(object.id)) {
				continue;
			}
			if (transformChanged) {
				if (roots.contains(object.id)) {
					object.transform->translate = MathUtility::Add(object.transform->translate, MathUtility::Subtract(edited.translate, original.translate));
					object.transform->rotate = MathUtility::Add(object.transform->rotate, MathUtility::Subtract(edited.rotate, original.rotate));
					object.transform->scale = MathUtility::Add(object.transform->scale, MathUtility::Subtract(edited.scale, original.scale));
				}
			}
			if (activeChanged) {
				*object.active = active;
			}
			if (colorChanged) {
				*object.color = color;
			}
		}
	}
	ImGui::End();
#else
	(void)scene;
#endif
}
