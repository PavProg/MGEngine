#ifndef BENCH_CONFIG_HPP
#define BENCH_CONFIG_HPP

#include <string>

// параметры запуска из к.с.: MyGameEngine.exe --bench=load-async --novsync
struct BenchConfig {
	std::string scenario;
	bool vsync = true;
};

#endif