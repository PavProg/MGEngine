#include "application/application.hpp"
#include "application/BenchConfig.hpp"

#include <string>

int main(int argc, char** argv) {
	BenchConfig bench;
	for (int i; i < argc; ++i) {
		std::string arg = argv[i];
		if (arg.rfind("--bench", 0) == 0) bench.scenario = arg.substr(8);
		else if (arg == "--novsync") bench.vsync = false;
	}

	Application app;
	if (!app.Init(1000, 800, "My game Engine", bench)) {
		return -1;
	}

	app.Run();
	return 0;
}