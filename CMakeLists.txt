cmake_minimum_required(VERSION 3.15)
project(MyGameEngine VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/application/application.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/gl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/logger/logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/states/MenuState.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/states/PlayState.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/states/PauseState.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/states/StateManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/OpenGLAdapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ecs/World.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ecs/systems/RenderSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ecs/systems/AnimationSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ecs/systems/HierarchyUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/SceneLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/ComponentRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/resources/ResourceManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/resources/MeshLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/resources/TextureLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/resources/ShaderLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/resources/StbImage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/resources/TinyObjLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ecs/systems/PhysicsSystem.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

option(MGE_ENABLE_PROFILER "Профилирование через Tracy" ON)

if(MGE_ENABLE_PROFILER)
    find_package(Tracy CONFIG REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE Tracy::TracyClient)
    target_compile_definitions(${PROJECT_NAME} PRIVATE MGE_PROFILER_ENABLED)
endif()

if(APPLE)
    target_include_directories(${PROJECT_NAME} PRIVATE /usr/local/include)

    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(GLFW QUIET glfw3)
    endif()

    if(GLFW_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${GLFW_LIBRARIES})
        target_link_directories(${PROJECT_NAME} PRIVATE ${GLFW_LIBRARY_DIRS})
    else()
        target_link_directories(${PROJECT_NAME} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/library)
        target_link_libraries(${PROJECT_NAME} PRIVATE glfw.3.4)
    endif()

    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework IOKit"
    )

elseif(WIN32)
    # --- Windows (vcpkg + MSVC / MinGW) ---
    find_package(glfw3          CONFIG REQUIRED)
    find_package(glm            CONFIG REQUIRED)

    target_link_libraries(${PROJECT_NAME} PRIVATE
        glfw
        glm::glm
        opengl32
        gdi32
    )

else()
    find_package(glfw3  CONFIG REQUIRED)
    find_package(glm    CONFIG REQUIRED)
    find_package(OpenGL REQUIRED)

    target_link_libraries(${PROJECT_NAME} PRIVATE
        glfw
        glm::glm
        OpenGL::GL
        dl
        pthread
    )
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    COMMENT "Copying assets to build directory"
)
