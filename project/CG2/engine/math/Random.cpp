#include "Random.h"

//=============================================================================================//
// Getter / 乱数エンジン

std::mt19937& Random::GetEngine() {
	// 共有している乱数エンジンを取得する。
	return engine_;
}

//=============================================================================================//
// 乱数生成


float Random::Range(float min, float max) {
	// 指定した範囲の一様分布から乱数を一つ得る。
	std::uniform_real_distribution<float> distribution(min, max);

	return distribution(engine_);
}
