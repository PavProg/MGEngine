#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include <memory>
#include "render/IRenderAdapter.hpp"
#include "ecs/World.hpp"
#include "ecs/SystemScheduler.hpp"
#include "scene/SceneSettings.hpp"
#include "resources/ResourceManager.hpp"

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
};

#endif