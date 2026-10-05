#include "LogSystem.h"
#include "StringUtility.h"

#include <Windows.h>
#include <chrono>
#include <filesystem>
#include <format>
#include <strsafe.h>
#include <dbgHelp.h>

#pragma comment(lib,"Dbghelp.lib")

//=============================================================================================//
// static変数実体

std::ofstream LogSystem::logStream_;

//=============================================================================================//
// 初期化

void LogSystem::Initialize () {

	// 誰も捕捉しなかった例外を補足する関数を登録
	SetUnhandledExceptionFilter (ExportDump);

	// logsディレクトリを掘る
	std::filesystem::create_directory ("logs");

	// 現在時刻を取得
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now ();

	// 秒単位に変換
	std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
		nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);

	// ローカル時間へ変換
	std::chrono::zoned_time localTime{
		std::chrono::current_zone (),
		nowSeconds
	};

	// 日付文字列作成
	std::string dateString = std::format ("{:%Y%m%d_%H%M%S}", localTime);

	// LogFilePath作成
	std::string logFilePath = std::string ("logs/") + dateString + ".log";

	// LogFileを開く
	logStream_.open (logFilePath);
}

//=============================================================================================//
// 終了処理

void LogSystem::Finalize () {

	// LogFileを閉じる
	if (logStream_.is_open ()) {
		logStream_.close ();
	}
}

//=============================================================================================//
// Log出力

void LogSystem::Log (const std::string& message) {

	// 出力ウィンドウに表示
	OutputDebugStringA (message.c_str ());

	// LogFileにも出力
	if (logStream_.is_open ()) {
		logStream_ << message;
	}
}

void LogSystem::Log (const std::wstring& message) {

	// wstring -> stringへ変換してLog
	Log (StringUtility::ConvertString (message));
}

void LogSystem::Log (std::ostream& os, const std::string& message) {

	// 指定されたostreamへ出力
	os << message << std::endl;

	// 出力ウィンドウへも表示
	OutputDebugStringA (message.c_str ());
}

//=============================================================================================//
// Getter

std::ofstream& LogSystem::GetLogStream () {
	return logStream_;
}

//=============================================================================================//
// Dump出力

LONG WINAPI LogSystem::ExportDump (EXCEPTION_POINTERS* exception) {

	// 時刻を取得
	SYSTEMTIME time;
	GetLocalTime (&time);

	// Dumpsディレクトリ作成
	CreateDirectory (L"./Dumps", nullptr);

	// DumpFilePath作成
	wchar_t filePath[MAX_PATH] = { 0 };

	StringCchPrintfW (
		filePath,
		MAX_PATH,
		L"./Dumps/%04d-%02d%02d-%02d%02d.dmp",
		time.wYear,
		time.wMonth,
		time.wDay,
		time.wHour,
		time.wMinute);

	// DumpFile生成
	HANDLE dumpFileHandle = CreateFile (
		filePath,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_WRITE | FILE_SHARE_READ,
		0,
		CREATE_ALWAYS,
		0,
		0);

	// Process情報取得
	DWORD processId = GetCurrentProcessId ();
	DWORD threadId = GetCurrentThreadId ();

	// MiniDump情報設定
	MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{};

	minidumpInformation.ThreadId = threadId;
	minidumpInformation.ExceptionPointers = exception;
	minidumpInformation.ClientPointers = TRUE;

	// Dump出力
	MiniDumpWriteDump (
		GetCurrentProcess (),
		processId,
		dumpFileHandle,
		MiniDumpNormal,
		&minidumpInformation,
		nullptr,
		nullptr);

	// DumpFileを閉じる
	CloseHandle (dumpFileHandle);

	return EXCEPTION_EXECUTE_HANDLER;
}