#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

// CPU時間と描画負荷の固定長履歴。記録中に文字列・配列の動的確保を行わない。
class FrameProfiler {
public:

	//=============================================================================================//
	// 型 / 計測区間・一フレームの記録

	enum class Section { 
		Editor, 
		History, 
		Project, 
		Hierarchy, 
		Inspector,
		SceneView,
		ModelLoad, 
		TextureLoad,
		SceneUpdate, 
		Motion,
		Physics, 
		Collision, 
		Models, 
		Animation,
		Particles,
		Previews,
		Render, 
		ImGui,
		Submit, 
		Present, 
		GpuWait, 
		FrameLimit, 
		Count 
	};

	struct Sample {
		uint64_t number = 0;

		float frameMilliseconds = 0;
		float gpuMilliseconds = 0; // 同じフレームのGPUタイムスタンプ差分。Present待機は含めない。

		bool isGpuTimingValid = false; // 未対応・取得失敗と0msを区別する。

		std::array<float, static_cast<size_t>(Section::Count)> milliseconds{};

		uint64_t drawCalls = 0; // Mesh経由の実際のD3D12描画命令数。インスタンス数とは分ける。
		uint64_t triangles = 0;
		uint64_t instances = 0;
		uint64_t uiDrawCalls = 0; // ImGuiの有効な描画コマンド。通常描画とは分離する。
	};

	// スコープの終了時に積算する。入れ子の区間は親の時間にも含まれる。
	class Scope {
	public:
		explicit Scope(Section section);

		~Scope();

		Scope(const Scope&) = delete;

		Scope& operator=(const Scope&) = delete;

	private:
		using Clock = std::chrono::steady_clock;

		Section section_; // 計測先の固定インデックス。

		Clock::time_point started_;
	};

	//=============================================================================================//
	// 初期化・更新処理 / フレーム境界

	static void Initialize();

	static void BeginFrame();

	static void EndFrame();

	//=============================================================================================//
	// Getter / 履歴・区間名

	static constexpr size_t kHistorySize = 600;

	static const Sample& GetLatest();

	static const Sample& GetPeak();

	static size_t GetCount();

	static const Sample& GetSample(size_t index);

	static bool IsPaused();

	static const char* GetSectionName(Section section);

	//=============================================================================================//
	// Setter / 描画カウンター・記録制御

	static void SetPaused(bool isPaused);

	static void ResetPeak();

	static void RecordDraw(uint32_t indices, uint32_t instances);

	static void RecordUiDraw();

	// フェンス完了後だけ呼ぶ。異常値は履歴へ混ぜず、未取得として表示する。
	static void SetGpuMilliseconds(float milliseconds);

	// 選択フレームの内訳も同じCSVへ保存し、後から原因を比較できるようにする。
	static bool ExportCsv(const std::string& path);

private:

	//=============================================================================================//
	// 固定長リングバッファ・計時

	using Clock = std::chrono::steady_clock;

	static Sample current_;
	static Sample latest_;
	static Sample peak_; // ピークはリングから消えても保持する。

	static std::array<Sample, kHistorySize> history_;

	static size_t count_;
	static size_t cursor_;

	static uint64_t number_;

	static bool isPaused_; // 計測表示だけを停止し、ゲームは止めない。

	static Clock::time_point started_;
};

// Sampleを含む型定義が揃った後で、共有データを一度だけ初期化する。
inline FrameProfiler::Sample FrameProfiler::current_{};

inline FrameProfiler::Sample FrameProfiler::latest_{};

inline FrameProfiler::Sample FrameProfiler::peak_{};

inline std::array<FrameProfiler::Sample, FrameProfiler::kHistorySize> FrameProfiler::history_{};

inline size_t FrameProfiler::count_ = 0;
inline size_t FrameProfiler::cursor_ = 0;

inline uint64_t FrameProfiler::number_ = 0;

inline bool FrameProfiler::isPaused_ = false;

inline FrameProfiler::Clock::time_point FrameProfiler::started_{};
