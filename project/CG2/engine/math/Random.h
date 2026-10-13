#pragma once
#include <random>

class Random
{
public:
	static std::mt19937& GetEngine() { return engine_; }

	static float Range(float min, float max);

private:
	static inline std::mt19937 engine_{ std::random_device{}() };

};

