#include "Random.h"

float Random::Range(float min, float max) {
	std::uniform_real_distribution<float> distribution(min, max);

	return distribution(engine_);
}