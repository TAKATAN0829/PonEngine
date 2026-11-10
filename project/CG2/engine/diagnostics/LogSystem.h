#pragma once

#include <fstream>
#include <string>
#include <Windows.h>
#include <deque>

class LogSystem {
public:

	//=============================================================================================//
	// 初期化

	static void Initialize();

	//=============================================================================================//
	// 終了処理

	static void Finalize();

	//=============================================================================================//
	// Log出力

	static void Log(const std::string& message);

	static void Log(const std::wstring& message);

	static void Log(std::ostream& os, const std::string& message);

	//=============================================================================================//
	// Getter

	static const std::deque<std::string>& GetRecentMessages();

private:

	//=============================================================================================//
	// Dump出力

	static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception);

private:

	//=============================================================================================//
	// LogFile

	// 全体で共有するログの書込先。
	inline static std::ofstream logStream_{};

	// Consoleへ渡す最近のログを保持する。
	inline static std::deque<std::string> recentMessages_{};
};
