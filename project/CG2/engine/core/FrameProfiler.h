#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <string>
#include <cmath>

// CPU時間と描画負荷の固定長履歴。記録中に文字列・配列の動的確保を行わない。
class FrameProfiler {
public:
	//=============================================================================================//
	// 型 / 計測区間・一フレームの記録

	enum class Section { Editor, History, Project, Hierarchy, Inspector, SceneView, ModelLoad, TextureLoad, SceneUpdate, Motion, Physics, Collision, Models, Animation, Particles, Previews, Render, ImGui, Submit, Present, GpuWait, FrameLimit, Count };
	struct Sample {
		uint64_t number = 0;
		float frameMilliseconds = 0;
		float gpuMilliseconds = 0; // 同じフレームのGPUタイムスタンプ差分。Present待機は含めない。
		bool gpuTimingValid = false; // 未対応・取得失敗と0msを区別する。
		std::array<float, static_cast<size_t>(Section::Count)> milliseconds{};
		uint64_t drawCalls = 0; // Mesh経由の実際のD3D12描画命令数。インスタンス数とは分ける。
		uint64_t triangles = 0;
		uint64_t instances = 0;
		uint64_t uiDrawCalls = 0; // ImGuiの有効な描画コマンド。通常描画とは分離する。
	};
	// スコープの終了時に積算する。入れ子の区間は親の時間にも含まれる。
	class Scope {
	public:
		explicit Scope(Section section) : section_(section), started_(Clock::now()) {}
		~Scope() {
			current_.milliseconds[static_cast<size_t>(section_)] += std::chrono::duration<float, std::milli>(Clock::now() - started_).count();
		}
		Scope(const Scope&) = delete;
		Scope& operator=(const Scope&) = delete;
	private:
		using Clock = std::chrono::steady_clock;
		Section section_; // 計測先の固定インデックス。
		Clock::time_point started_;
	};

	//=============================================================================================//
	// 初期化・更新処理 / フレーム境界

	static void Initialize() {
		current_ = {};
		latest_ = {};
		peak_ = {};
		history_ = {};
		count_ = 0;
		cursor_ = 0;
		number_ = 0;
		paused_ = false;
	}
	static void BeginFrame() {
		current_ = {};
		current_.number = ++number_;
		started_ = Clock::now();
	}
	static void EndFrame() {
		current_.frameMilliseconds = std::chrono::duration<float, std::milli>(Clock::now() - started_).count();
		if (paused_) {
			return;
		}
		latest_ = current_;
		if (latest_.frameMilliseconds > peak_.frameMilliseconds) {
			peak_ = latest_;
		}
		history_[cursor_] = latest_;
		cursor_ = (cursor_ + 1) % kHistorySize;
		if (count_ < kHistorySize) {
			++count_;
		}
	}

	//=============================================================================================//
	// Getter / 履歴・区間名

	static constexpr size_t kHistorySize = 600;
	static const Sample& GetLatest() { return latest_; }
	static const Sample& GetPeak() { return peak_; }
	static size_t GetCount() { return count_; }
	static const Sample& GetSample(size_t index) {
		const size_t first = (cursor_ + kHistorySize - count_) % kHistorySize;
		return history_[(first + index) % kHistorySize];
	}
	static bool IsPaused() { return paused_; }
	static const char* GetSectionName(Section section) {
		static constexpr const char* names[] = {"Editor UI / history (total)", "  History / shortcuts", "  Project / Console", "  Hierarchy", "  Inspector", "  SceneView UI / picking", "Model load (incl. textures)", "  Texture load", "Scene update (total)", "  Motion components", "  Physics / gravity / response", "  Collision detection", "  Models (incl. animation)", "    CPU animation", "  Particles", "  Asset previews", "Scene draw recording", "ImGui draw recording", "Submit / reset (total)", "  Present / VSync", "  GPU fence wait", "  FPS limiter"};
		return names[static_cast<size_t>(section)];
	}

	//=============================================================================================//
	// Setter・メンバ関数 / 描画カウンター・記録制御

	static void SetPaused(bool paused) { paused_ = paused; }
	static void ResetPeak() { peak_ = {}; }
	static void RecordDraw(uint32_t indices, uint32_t instances) {
		++current_.drawCalls;
		current_.triangles += static_cast<uint64_t>(indices / 3) * instances;
		current_.instances += instances;
	}
	static void RecordUiDraw() { ++current_.uiDrawCalls; }
	// フェンス完了後だけ呼ぶ。異常値は履歴へ混ぜず、未取得として表示する。
	static void SetGpuMilliseconds(float milliseconds) {
		current_.gpuTimingValid = false;
		current_.gpuMilliseconds = 0;
		if (!std::isfinite(milliseconds) || milliseconds < 0) {
			return;
		}
		current_.gpuMilliseconds = milliseconds;
		current_.gpuTimingValid = true;
	}
	// 選択フレームの内訳も同じCSVへ保存し、後から原因を比較できるようにする。
	static bool ExportCsv(const std::string& path) {
		std::ofstream output(path, std::ios::trunc);
		output << "frame,frame_ms,draw_calls,ui_draw_calls,triangles,instances,gpu_valid,gpu_ms";
		for (size_t sectionIndex = 0; sectionIndex < static_cast<size_t>(Section::Count); ++sectionIndex) {
			output << ',' << GetSectionName(static_cast<Section>(sectionIndex));
		}
		output << '\n';
		for (size_t sampleIndex = 0; sampleIndex < count_; ++sampleIndex) {
			const Sample& sample = GetSample(sampleIndex);
			output << sample.number << ',' << sample.frameMilliseconds << ',' << sample.drawCalls << ',' << sample.uiDrawCalls << ',' << sample.triangles << ',' << sample.instances;
			output << ',' << sample.gpuTimingValid << ',';
			if (sample.gpuTimingValid) {
				output << sample.gpuMilliseconds;
			}
			for (float duration : sample.milliseconds) {
				output << ',' << duration;
			}
			output << '\n';
		}
		output.close();
		return static_cast<bool>(output);
	}

private:
	//=============================================================================================//
	// メンバ変数 / 固定長リングバッファ・計時

	using Clock = std::chrono::steady_clock;
	static Sample current_;
	static Sample latest_;
	static Sample peak_; // ピークはリングから消えても保持する。
	static std::array<Sample, kHistorySize> history_;
	inline static size_t count_ = 0;
	inline static size_t cursor_ = 0;
	inline static uint64_t number_ = 0;
	inline static bool paused_ = false; // 計測表示だけを停止し、ゲームは止めない。
	inline static Clock::time_point started_{};
};

inline FrameProfiler::Sample FrameProfiler::current_{};
inline FrameProfiler::Sample FrameProfiler::latest_{};
inline FrameProfiler::Sample FrameProfiler::peak_{};
inline std::array<FrameProfiler::Sample, FrameProfiler::kHistorySize> FrameProfiler::history_{};
