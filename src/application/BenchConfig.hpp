#ifndef BENCH_CONFIG_HPP
#define BENCH_CONFIG_HPP

#include <string>

// параметры запуска из к.с.: MyGameEngine.exe --bench=load-async --novsync
struct BenchConfig {
#ifdef MGE_TRANSFORM_BENCH_AVAILABLE
	std::string scenario = "transform-parallel";
	bool vsync = false;
#else
	std::string scenario;
	bool vsync = true;
#endif

	static constexpr bool SupportsTransformBench() {
#ifdef MGE_TRANSFORM_BENCH_AVAILABLE
		return true;
#else
		return false;
#endif
	}

	bool IsTransformBench() const {
		return scenario == "transform-serial"
			|| scenario == "transform-parallel";
	}
};

#endif
