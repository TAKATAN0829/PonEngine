#pragma once

#include <random>

class Random
{
public:

	//=============================================================================================//
	// 乱数生成

	static float Range(float min, float max);

private:

	//=============================================================================================//
	// 乱数エンジン

	// 初回の乱数で種を決め、以降は同じエンジンを共有する。
	inline static std::mt19937 engine_{ std::random_device{}() };

};

