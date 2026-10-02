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
#include "ecs/components/Transform.hpp"
#include "ecs/components/Tag.hpp"
#include "ecs/components/MeshRenderer.hpp"
#include "ecs/systems/RenderSystem.hpp"
#include "ecs/systems/AnimationSystem.hpp"
#include "ecs/components/Hierarchy.hpp"
#include "ecs/systems/HierarchyUtils.hpp"

#include "scene/SceneLoader.hpp"

#include "resources/ResourceManager.hpp"
#include "resources/Mesh.hpp"
#include "resources/Texture.hpp"
#include "resources/ShaderProgram.hpp"

#include "ecs/components/Collider.hpp"
#include "ecs/components/Rigidbody.hpp"

#include "core/profiler.hpp"

Application::Application() 
: window(nullptr), running(false), lastFrameTime(0.0), stateManager(nullptr) {}

Application::~Application() {
    Shutdown();
}

bool Application::Init(int width, int height, const char* title) {

    Logger::Init("game.log");

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

    resourceManager = std::make_unique<ResourceManager>(*renderer);

    resourceManager->InitPlaceholders();
    resourceManager->StartAsync(2);

    world = std::make_unique<World>();
    Logger::Info("Build Marker v2: creating systems");
    physicsSystem = std::make_unique<PhysicsSystem>(*world);
    Logger::Info("PhysicsSystem created");
    renderSystem = std::make_unique<RenderSystem>(*world, *renderer);
    Logger::Info("RenderSystem created");
    animationSystem = std::make_unique<AnimationSystem>(*world);
    Logger::Info("AnimationSystem created");

    if (!SceneLoader::Load("assets/scenes/test_scene.json", *world, m_sceneSettings)) {
        Logger::Error("Failed to load scene; using empty world");
    }

    CreateDemoLoadedEntities();

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
        { MGE_PROFILE_SCOPE_N("PumpUploads"); if (resourceManager) resourceManager->PumpUploads(); }

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
                    { MGE_PROFILE_SCOPE_N("Physics"); if (physicsSystem) physicsSystem->Update(SIM_STEP); }
                    { MGE_PROFILE_SCOPE_N("Animation"); if (animationSystem) animationSystem->Update(SIM_STEP); }
                    
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

            { MGE_PROFILE_SCOPE_N("RenderSystem"); if (renderSystem) renderSystem->Update(); }
            { MGE_PROFILE_SCOPE_N("StateRender"); if (stateManager) stateManager->Render(*renderer); }
        }

        { MGE_PROFILE_SCOPE_N("Swap"); renderer->SwapBuffers(); }
        MGE_PROFILE_FRAME();
    }
    Logger::Info("Exited main loop");
}

void Application::CreateDemoLoadedEntities() {
    // Загружаем 3 типа ресурсов
    auto mesh = resourceManager->LoadAsync<Mesh>("assets/models/quad.obj");
    auto texture = resourceManager->LoadAsync<Texture>("assets/textures/test.png");
    auto shader = resourceManager->Load<ShaderProgram>("assets/shaders/model");

    if (!mesh || !texture || !shader) {
        Logger::Error("Demo: one of the resources failed to load");
        return;
    }

    // Спавним 3 сущности
    for (int i = 0; i < 2; ++i) {
        EntityId e = world->CreateEntity();

        Transform t{};
        t.position = glm::vec3(-3.0f + i * 3.0f, -3.0f, 0.0f);
        t.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        t.scale = glm::vec3(2.0f);
        world->AddComponent<Transform>(e, t);

        MeshRenderer mr{};
        mr.primitiveType = PrimitiveType::None;
        mr.mesh = mesh;
        mr.texture = texture;
        mr.shader = shader;
        mr.tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        world->AddComponent<MeshRenderer>(e, mr);

        world->AddComponent<Tag>(e, { "LoadedQuad_" + std::to_string(i) });

        Rigidbody rb{};
        rb.velocity = glm::vec3(0.0f, 0.0f, 0.0f);
        rb.useGravity = false;
        world->AddComponent<Rigidbody>(e, rb);

        Collider col{};
        col.type = ColliderType::Box;
        col.halfExtents = glm::vec3(0.5f, 0.5f, 0.5f);
        world->AddComponent<Collider>(e, col);
    }

    for (int i = 0; i < 1; ++i) {
        EntityId e = world->CreateEntity();

        Transform t{};
        t.position = glm::vec3(-3.0f + i * 3.0f, -3.0f, 0.0f);
        t.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        t.scale = glm::vec3(2.0f);
        world->AddComponent<Transform>(e, t);

        MeshRenderer mr{};
        mr.primitiveType = PrimitiveType::None;
        mr.mesh = mesh;
        mr.texture = texture;
        mr.shader = shader;
        mr.tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        world->AddComponent<MeshRenderer>(e, mr);

        world->AddComponent<Tag>(e, { "LoadedQuad_" + std::to_string(i) });

        Rigidbody rb{};
        rb.velocity = glm::vec3(0.0f, 0.0f, 0.0f);
        rb.useGravity = false;
        world->AddComponent<Rigidbody>(e, rb);

        Collider col{};
        col.type = ColliderType::Box;
        col.halfExtents = glm::vec3(0.5f, 0.5f, 0.5f);
        world->AddComponent<Collider>(e, col);
    }

    Logger::Info("Demo: 3 entities sharing one mesh + one texture + one shader");
}

void Application::Shutdown() {
    Logger::Warning("Shutting down application");

    // 1. Сначала потоки
    if (resourceManager) resourceManager->StopAsync();

    // 2. ECS
    animationSystem.reset();
    renderSystem.reset();
    world.reset();

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

    Logger::ShutDown();
}