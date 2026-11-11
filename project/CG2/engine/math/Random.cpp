#include "Random.h"

//=============================================================================================//
// 乱数生成


float Random::Range(float min, float max) {
	// 指定した範囲の一様分布から乱数を一つ得る。
	// 共通の乱数器から、呼び出しごとに指定された範囲の値を取り出す。
	std::uniform_real_distribution<float> distribution(min, max);

	return distribution(engine_);
}
