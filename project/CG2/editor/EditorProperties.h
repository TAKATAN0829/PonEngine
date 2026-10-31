#pragma once
#include "EngineStructs.h"

// Inspectorのラベル列・入力列を揃える。Beginに成功した場合だけEndを呼ぶ。
class EditorProperties {
public:
	//=============================================================================================//
	// 描画処理 / セクション・プロパティ表

	static bool Header(const char* title, const char* help = nullptr);
	static bool Begin(const char* identifier);
	static void Row(const char* label);
	static void End();

	//=============================================================================================//
	// 描画処理 / 値の編集。戻り値は変更の有無。

	static bool Float(const char* label, float* value, float speed, float minimum, float maximum, const char* format = "%.2f");
	static bool Vector(const char* label, Vector3* value, float speed, float minimum = 0, float maximum = 0);
	static bool Toggle(const char* label, bool* value);
	static bool Combo(const char* label, int* value, const char* choices);
	static bool Color(const char* label, Vector4* value);
};
