#pragma once

#include <fstream>
#include <string>
#include <Windows.h>

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
	// 文字列変換

	static std::wstring ConvertString(const std::string& str);

	static std::string ConvertString(const std::wstring& str);

	//=============================================================================================//
	// Getter

	static std::ofstream& GetLogStream();

private:

	//=============================================================================================//
	// Dump出力

	static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception);

private:

	//=============================================================================================//
	// LogFile

	static std::ofstream logStream_;
};