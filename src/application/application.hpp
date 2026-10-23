#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include <memory>
#include <vector>
#include "render/IRenderAdapter.hpp"
#include "ecs/World.hpp"
#include "ecs/SystemScheduler.hpp"
#include "scene/SceneSettings.hpp"
#include "resources/ResourceManager.hpp"
#include "core/JobSystem.hpp"

#include "application/BenchConfig.hpp"

struct GLFWwindow;
class StateManager;

class Application {
    public:
        Application();
        ~Application();

        bool Init(int width, int height, const char* title, const BenchConfig& bench = {});
        void Run();
        void Shutdown();

    private:
        GLFWwindow* window;
        bool running;
        double lastFrameTime;
        std::unique_ptr<JobSystem> jobs;
        std::unique_ptr<StateManager> stateManager;
        std::unique_ptr<IRenderAdapter> renderer;
        std::unique_ptr<World> world;
        std::unique_ptr<SystemScheduler> scheduler;     // все ECS-системы живут здесь

        std::unique_ptr<ResourceManager> resourceManager;

        void RegisterComponents();
        void CreateDemoLoadedEntities();

        SceneSettings m_sceneSettings;

        float m_simAccumulator = 0.0f;
        float m_simSpeed = 1.0f;
        static constexpr float SIM_STEP = 1.0f / 60.0f;

        float m_hotReloadAccumulator = 0.0f;
        static constexpr float HOT_RELOAD_INTERVAL = 0.5f;
        // Сколько главный поток тратит на выгрузку на Gpu за кадр
        static constexpr double UPLOAD_BUDGET_MS = 3.0;

        // Режим замера
        enum class BenchPhase { Off, Warmup, Loading, Tail, Done };

        BenchConfig m_bench;
        BenchPhase  m_benchPhase = BenchPhase::Off;
        double      m_benchT0 = 0.0;
        double      m_benchBatchStart = 0.0;
        double      m_benchTailStart = 0.0;
        // Держим ресурсы: кэш хранит только weak_ptr и иначе сразу их выгрузит
        std::vector<std::shared_ptr<Resource<Texture>>> m_benchTextures;
        std::vector<std::shared_ptr<Resource<Mesh>>>    m_benchMeshes;

        static constexpr double BENCH_WARMUP_S = 3.0;    // прогрев до загрузки
        static constexpr double BENCH_TAIL_S   = 3.0;    // "хвост" после загрузки
        static constexpr double BENCH_STEADY_S = 15.0;   // длительность сценария steady

        void UpdateBench(double now);
        void StartBenchBatch();
        bool BenchBatchDone() const;
};

#endif