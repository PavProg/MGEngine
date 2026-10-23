#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "application.hpp"
#include "logger/logger.hpp"

#include "states/StateManager.hpp"
#include "states/MenuState.hpp"
#include "states/PlayState.hpp"
#include "states/PauseState.hpp"
#include "render/OpenGLAdapter.hpp"

#include "ecs/World.hpp"
#include "ecs/SystemScheduler.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/components/WorldTransform.hpp"
#include "ecs/components/Tag.hpp"
#include "ecs/components/MeshRenderer.hpp"
#include "ecs/components/Hierarchy.hpp"
#include "ecs/systems/RenderSystem.hpp"
#include "ecs/systems/AnimationSystem.hpp"
#include "ecs/systems/TransformSystem.hpp"
#include "ecs/systems/HierarchyUtils.hpp"

#include "scene/SceneLoader.hpp"

#include "resources/ResourceManager.hpp"
#include "resources/Mesh.hpp"
#include "resources/Texture.hpp"
#include "resources/ShaderProgram.hpp"

#include "core/profiler.hpp"

#include <cstdio>

Application::Application()
: window(nullptr), running(false), lastFrameTime(0.0), stateManager(nullptr) {}

Application::~Application() {
    Shutdown();
}

bool Application::Init(int width, int height, const char* title, const BenchConfig& bench) {
    m_bench = bench;

    Logger::Init("game.log");

    // Самой первой подсистемой и строго из главного потока:
    // поток, который вызвал Init, enkiTS считает потоком 0
    jobs = std::make_unique<JobSystem>();
    const unsigned hw = std::thread::hardware_concurrency();
    jobs->Init(hw > 2 ? hw - 2 : 1);

    if (!glfwInit()) {
        Logger::Error("Failed to initialize GLFW");
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    #ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif

    window = glfwCreateWindow(width, height, title, nullptr, nullptr);

    if (!window) {
        Logger::Error("Failed to create GLFW window");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGL(glfwGetProcAddress)) {
        Logger::Error("Failed to initialize GLAD");
        glfwDestroyWindow(window);
        glfwTerminate();
        return false;
    }
    // 1 - ждать монитор (VSync), 0 - рисовать без ожидания (для замеров)
    glfwSwapInterval(m_bench.vsync ? 1 : 0);

    running = true;
    Logger::Info("Application initialized");

    stateManager = std::make_unique<StateManager>();
    stateManager->ChangeState(std::make_unique<MenuState>());
    Logger::Info("After ChangeState in Init");

    renderer = std::make_unique<OpenGLAdapter>(window);
    if (!renderer->Init()) {
        Logger::Error("Failed to initialize render adapter");
        return false;
    }

    resourceManager = std::make_unique<ResourceManager>(*renderer, *jobs);

    resourceManager->InitPlaceholders();

    world = std::make_unique<World>();
    RegisterComponents();

    // Фаза каждой системы задана в ней самой (GetPhase)
    // Физика временно отключена: ее заменит физ. движок
    scheduler = std::make_unique<SystemScheduler>(*world);
    scheduler->AddSystem<AnimationSystem>();
    scheduler->AddSystem<TransformSystem>();
    scheduler->AddSystem<RenderSystem>(*renderer);
    Logger::Info("ECS systems created");

    if (!SceneLoader::Load("assets/scenes/test_scene.json", *world, m_sceneSettings)) {
        Logger::Error("Failed to load scene; using empty world");
    }

    CreateDemoLoadedEntities();

    if (!m_bench.scenario.empty()) {
        m_benchPhase = BenchPhase::Warmup;
        m_benchT0 = glfwGetTime();
        Logger::Info("BENCH: scenario " + m_bench.scenario
            + (m_bench.vsync ? " (vsync on)" : " (vsync off)"));
    }

    return true;
}

void Application::Run() {   // Внутри разметка под tracy. Размечаем фазы кадра, системы, все что может блокнуться и циклы (целиком).
    MGE_PROFILE_THREAD("Main");

    Logger::Info("Entering main loop");

    lastFrameTime = glfwGetTime();
    m_simAccumulator = 0.0f;

    while (running && !glfwWindowShouldClose(window)) {
        MGE_PROFILE_SCOPE_N("Frame");

        double currentTime = glfwGetTime();
        float realDelta = static_cast<float>(currentTime - lastFrameTime);
        lastFrameTime = currentTime;

        // Защита от spiral of death
        if (realDelta > 0.25f) realDelta = 0.25f;

        { MGE_PROFILE_SCOPE_N("PollEvents"); glfwPollEvents(); }
        UpdateBench(currentTime);
        // Ввод - раз в кадр отрисовки (повороту камеры мышью нужна частота монитора)
        scheduler->RunPhase(UpdatePhase::Input, realDelta);
        { MGE_PROFILE_SCOPE_N("PumpUploads"); if (resourceManager) resourceManager->PumpUploads(UPLOAD_BUDGET_MS); }
        if (jobs) {
            jobs->CollectFinished();
            MGE_PROFILE_PLOT("JobsInFlight", static_cast<int64_t>(jobs->GetJobsInFlight()));
        }
        if (resourceManager) {
            MGE_PROFILE_PLOT("LoadsInFlight", static_cast<int64_t>(resourceManager->GetLoadsInFlight()));
            MGE_PROFILE_PLOT("FinalizeQueue", static_cast<int64_t>(resourceManager->GetPendingFinalizers()));
        }

        {
            MGE_PROFILE_SCOPE_N("HotReload");
            m_hotReloadAccumulator += realDelta;
            if (m_hotReloadAccumulator >= HOT_RELOAD_INTERVAL) {
                m_hotReloadAccumulator = 0.0f;
                if (resourceManager) {
                    resourceManager->ReloadModifiedShaders();
                    resourceManager->ReloadModifiedTextures();
                }
            }
        }

        // Phase SIM
        int simSteps = 0;
        {
            MGE_PROFILE_SCOPE_NC("Simulation", 0xDC143C);   // Логика - красным
            m_simAccumulator += realDelta * m_simSpeed;
            while (m_simAccumulator >= SIM_STEP) {
                MGE_PROFILE_SCOPE_N("SimStep");
                    { MGE_PROFILE_SCOPE_N("StateLogic"); if (stateManager) stateManager->Update(SIM_STEP); }
                    scheduler->RunPhase(UpdatePhase::GameLogic, SIM_STEP);
                    scheduler->RunPhase(UpdatePhase::Animation, SIM_STEP);
                    scheduler->RunPhase(UpdatePhase::Physics, SIM_STEP);
                    scheduler->RunPhase(UpdatePhase::PostPhysics, SIM_STEP);

                    m_simAccumulator -= SIM_STEP;
                    ++simSteps;
            }
        }
        MGE_PROFILE_PLOT("SimStepsPerFrame", static_cast<int64_t>(simSteps));

        {            // Настройка рендера для этого кадра
            MGE_PROFILE_SCOPE_NC("Render", 0x4169E1); // Рендер- синим

            int fbWidth, fbHeight;
            glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
            glViewport(0, 0, fbWidth, fbHeight);

            float aspect = (fbHeight > 0)
                ? static_cast<float>(fbWidth) / static_cast<float>(fbHeight)
                : 1.0f;

            renderer->Clear(
                m_sceneSettings.background.r,
                m_sceneSettings.background.g,
                m_sceneSettings.background.b,
                m_sceneSettings.background.a
            );

            glm::mat4 view = glm::lookAt(
                m_sceneSettings.camera.position,
                m_sceneSettings.camera.target,
                m_sceneSettings.camera.up
            );

            glm::mat4 projection;
            if (m_sceneSettings.camera.projection == ProjectionType::Orthographic) {
                float halfH = m_sceneSettings.camera.orthoSize;
                float halfW = halfH * aspect;
                // ortho - это матрица, которая создает параллепипед с 4 аргументами: лева, права, вверх, низ, nearPlan, farPlan
                projection = glm::ortho(-halfW, halfW, -halfH, halfH, m_sceneSettings.camera.nearPlan, m_sceneSettings.camera.farPlan);
            }
            else {
                projection = glm::perspective(glm::radians(m_sceneSettings.camera.fov),
                    aspect,
                    m_sceneSettings.camera.nearPlan, m_sceneSettings.camera.farPlan); // (угол обзора, соотношение экрана, nearPlan, farPlan)
            }

            renderer->SetCamera(view, projection);

            // Мировые матрицы -> звук -> отрисовка (раз в кадр)
            scheduler->RunPhase(UpdatePhase::Transform, realDelta);
            scheduler->RunPhase(UpdatePhase::Audio, realDelta);
            scheduler->RunPhase(UpdatePhase::Render, realDelta);
            { MGE_PROFILE_SCOPE_N("StateRender"); if (stateManager) stateManager->Render(*renderer); }
        }

        { MGE_PROFILE_SCOPE_N("Swap"); renderer->SwapBuffers(); }
        MGE_PROFILE_FRAME();
    }
    Logger::Info("Exited main loop");
}

// Каждый тип компонента регистрируется один раз, до создания сущностей
void Application::RegisterComponents() {
    world->RegisterComponent<Transform>();
    world->RegisterComponent<WorldTransform>();
    world->RegisterComponent<Hierarchy>();
    world->RegisterComponent<MeshRenderer>();
    world->RegisterComponent<Tag>();
}

void Application::CreateDemoLoadedEntities() {
    // Загружаем 3 типа ресурсов
    auto mesh = resourceManager->LoadAsync<Mesh>("assets/models/quad.obj", JobPriority::High);
    auto texture = resourceManager->LoadAsync<Texture>("assets/textures/test.png", JobPriority::High);
    auto shader = resourceManager->Load<ShaderProgram>("assets/shaders/model");

    if (!mesh || !texture || !shader) {
        Logger::Error("Demo: one of the resources failed to load");
        return;
    }

    // Спавним 3 сущности
    // Rigidbody/Collider убраны вместе с физикой (вернутся с физ. движком)
    for (int i = 0; i < 2; ++i) {
        Entity e = world->CreateEntity();

        Transform t{};
        t.position = glm::vec3(-3.0f + i * 3.0f, -3.0f, 0.0f);
        t.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        t.scale = glm::vec3(2.0f);
        world->AddComponent<Transform>(e, t);
        world->AddComponent<WorldTransform>(e, WorldTransform{});

        MeshRenderer mr{};
        mr.primitiveType = PrimitiveType::None;
        mr.mesh = mesh;
        mr.texture = texture;
        mr.shader = shader;
        mr.tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        world->AddComponent<MeshRenderer>(e, mr);

        world->AddComponent<Tag>(e, { "LoadedQuad_" + std::to_string(i) });
    }

    for (int i = 0; i < 1; ++i) {
        Entity e = world->CreateEntity();

        Transform t{};
        t.position = glm::vec3(-3.0f + i * 3.0f, -3.0f, 0.0f);
        t.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        t.scale = glm::vec3(2.0f);
        world->AddComponent<Transform>(e, t);
        world->AddComponent<WorldTransform>(e, WorldTransform{});

        MeshRenderer mr{};
        mr.primitiveType = PrimitiveType::None;
        mr.mesh = mesh;
        mr.texture = texture;
        mr.shader = shader;
        mr.tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        world->AddComponent<MeshRenderer>(e, mr);

        world->AddComponent<Tag>(e, { "LoadedQuad_" + std::to_string(i) });
    }

    Logger::Info("Demo: 3 entities sharing one mesh + one texture + one shader");
}

// --- Режим замера ---

void Application::StartBenchBatch() {
    MGE_PROFILE_SCOPE_N("BenchSubmit");
    const bool sync = (m_bench.scenario == "load-sync");
    char path[64];
    for (int i = 0; i < 24; ++i) {
        std::snprintf(path, sizeof(path), "assets/bench/tex_%02d.png", i);
        m_benchTextures.push_back(sync ? resourceManager->Load<Texture>(path)
                                       : resourceManager->LoadAsync<Texture>(path));
    }
    for (int i = 0; i < 8; ++i) {
        std::snprintf(path, sizeof(path), "assets/bench/mesh_%02d.obj", i);
        m_benchMeshes.push_back(sync ? resourceManager->Load<Mesh>(path)
                                     : resourceManager->LoadAsync<Mesh>(path));
    }
    Logger::Info("BENCH: batch submitted");   // логгер дублирует сообщение в Tracy
}

bool Application::BenchBatchDone() const {
    auto loading = [](const auto& r) { return r && r->state.load() == ResourceState::Loading; };
    for (const auto& t : m_benchTextures) if (loading(t)) return false;
    for (const auto& m : m_benchMeshes)   if (loading(m)) return false;
    return true;
}

// Прогрев -> пачка загрузок -> ожидание готовности -> "хвост" -> выход.
// Время по часам (glfwGetTime), а не по кадрам: прогоны совпадают при любом FPS.
void Application::UpdateBench(double now) {
    switch (m_benchPhase) {
    case BenchPhase::Off:
        return;

    case BenchPhase::Warmup:
        if (now - m_benchT0 < BENCH_WARMUP_S) return;
        if (m_bench.scenario == "steady") {
            m_benchPhase = BenchPhase::Tail;
            m_benchTailStart = now;
            return;
        }
        m_benchBatchStart = now;
        StartBenchBatch();
        m_benchPhase = BenchPhase::Loading;
        return;

    case BenchPhase::Loading:
        if (m_bench.scenario == "load-exit" && now - m_benchBatchStart >= 0.1) {
            Logger::Info("BENCH: exit requested with live loads");
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            m_benchPhase = BenchPhase::Done;
        }
        else if (BenchBatchDone()) {
            Logger::Info("BENCH: batch done in "
                + std::to_string((now - m_benchBatchStart) * 1000.0) + " ms");
            m_benchPhase = BenchPhase::Tail;
            m_benchTailStart = now;
        }
        return;

    case BenchPhase::Tail: {
        const double tail = (m_bench.scenario == "steady") ? BENCH_STEADY_S : BENCH_TAIL_S;
        if (now - m_benchTailStart >= tail) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            m_benchPhase = BenchPhase::Done;
        }
        return;
    }

    case BenchPhase::Done:
        return;
    }
}

void Application::Shutdown() {
    MGE_PROFILE_SCOPE_N("Shutdown");
    Logger::Warning("Shutting down application");

    // 1. Сначала потоки
    if (jobs) jobs->Shutdown();
    if (resourceManager) resourceManager->DropPendingUploads();

    // 2. ECS: системы раньше мира (они держат ссылки на World и рендер)
    scheduler.reset();
    world.reset();

    // Ресурсы замера освобождают GPU-объекты через рендер - отпускаем их, пока рендер жив
    m_benchTextures.clear();
    m_benchMeshes.clear();

    // 3. Менеджер
    resourceManager.reset();

    // 4. Рендер
    if (renderer) {
        renderer->Shutdown();
        renderer.reset();
    }

    stateManager.reset();
    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }
    glfwTerminate();
    running = false;

    jobs.reset();

    Logger::ShutDown();
}