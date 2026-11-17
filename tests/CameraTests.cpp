#include "ecs/World.hpp"
#include "ecs/CommandBuffer.hpp"
#include "ecs/components/Camera.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/components/WorldTransform.hpp"
#include "ecs/components/Hierarchy.hpp"
#include "ecs/components/Tag.hpp"
#include "ecs/components/FreeFlyCameraController.hpp"
#include "ecs/systems/CameraSystem.hpp"
#include "ecs/systems/TransformSystem.hpp"
#include "ecs/systems/CameraControllerSystem.hpp"
#include "input/InputFrame.hpp"
#include "render/IRenderAdapter.hpp"
#include "scene/sceneLoader.hpp"
#include "logger/logger.hpp"
#include <cmath>
#include <stdexcept>
#include <iostream>
struct Renderer : IRenderAdapter {
    glm::mat4 view{1}, projection{1}; int calls = 0;
    bool Init() override { return true; }
    void Clear(float,float,float,float) override {}
    void SwapBuffers() override {}
    void Shutdown() override {}
    void DrawMesh(const glm::mat4&,PrimitiveType,const glm::vec4&) override {}
    void SetCamera(const glm::mat4& v,const glm::mat4& p) override { view=v; projection=p; ++calls; }
    void ReloadShaders() override {}
    GPUMesh UploadMesh(const MeshData&) override { return {}; }
    void ReleaseMesh(GPUMesh&) override {}
    GPUTexture UploadTexture(const TextureData&) override { return {}; }
    void ReleaseTexture(GPUTexture&) override {}
    GPUShader CompileShader(const std::string&,const std::string&) override { return {}; }
    void ReleaseShader(GPUShader&) override {}
    void DrawLoadedMesh(const GPUMesh&,const GPUTexture&,const GPUShader&,const glm::mat4&,const glm::vec4&) override {}
};
void check(bool value) { if (!value) throw std::runtime_error("Camera check failed"); }
bool near(float a,float b) { return std::abs(a-b)<0.0001f; }
int main() {
    Logger::Init("camera-tests.log");
    World w;
    w.RegisterComponent<Camera>(); w.RegisterComponent<Transform>();
    w.RegisterComponent<WorldTransform>(); w.RegisterComponent<Hierarchy>();
    w.RegisterComponent<Tag>(); w.RegisterComponent<MeshRenderer>();
    w.RegisterComponent<FreeFlyCameraController>();
    Entity a=w.CreateEntity();
    w.AddComponent<Camera>(a,Camera{});
    Transform pose; pose.position={0,0,5};
    w.AddComponent<Transform>(a,pose); w.AddComponent<WorldTransform>(a,{});
    Renderer renderer; CameraContext context; context.activeCamera=a;
    context.framebufferWidth=800; context.framebufferHeight=400;
    CameraSystem camera(renderer,context); TransformSystem transform; CommandBuffer commands;
    transform.Update(w,commands,0); camera.Update(w,commands,0);
    check(context.ready && near(renderer.view[3].z,-5));
    check(near(renderer.projection[1][1]/renderer.projection[0][0],2));
    auto& lens=w.GetComponent<Camera>(a);
    for (float z : {-lens.nearPlane,-lens.farPlane}) {
        auto clip=renderer.projection*glm::vec4(0,0,z,1);
        check(near(clip.z/clip.w,z==-lens.nearPlane ? -1.f : 1.f));
    }
    context.framebufferWidth=400; camera.Update(w,commands,0);
    check(near(renderer.projection[0][0],renderer.projection[1][1]));
    lens.projection=ProjectionType::Orthographic; camera.Update(w,commands,0);
    check(near(renderer.projection[1][1],0.1f));
    context.framebufferHeight=0; int calls=renderer.calls; camera.Update(w,commands,0);
    check(!context.ready && renderer.calls==calls); context.framebufferHeight=400;
    lens.nearPlane=-1; camera.Update(w,commands,0); check(!context.ready); lens.nearPlane=0.1f;
    Entity parent=w.CreateEntity(); pose.position={3,0,0};
    w.AddComponent<Transform>(parent,pose); w.AddComponent<WorldTransform>(parent,{});
    Hierarchy h; h.parent=parent; w.AddComponent<Hierarchy>(a,h);
    transform.Update(w,commands,0); camera.Update(w,commands,0); check(near(renderer.view[3].x,-3));
    context.activeCamera=parent; camera.Update(w,commands,0); check(!context.ready);
    context.activeCamera=a; w.RemoveComponent<Hierarchy>(a);
    w.AddComponent<FreeFlyCameraController>(a,{});
    InputFrame input; input.focused=true; input.cursorCaptured=true;
    CameraControllerSystem controller(input,context.activeCamera);
    input.down.set(static_cast<size_t>(InputKey::W)); controller.Update(w,commands,1);
    check(w.GetComponent<Transform>(a).position.z<5);
    input.mouseDelta={20,10}; controller.Update(w,commands,0);
    check(std::abs(w.GetComponent<Transform>(a).rotation.y)>0.001f);
    w.DestroyEntity(a); camera.Update(w,commands,0); check(!context.ready);
    SceneSettings settings;
    check(SceneLoader::Load("assets/scenes/test_scene.json",w,settings));
    check(w.IsAlive(settings.activeCamera));
    context.activeCamera=settings.activeCamera;
    transform.Update(w,commands,0); camera.Update(w,commands,0);
    check(context.ready && near(renderer.view[3].z,-20));
    check(SceneLoader::Load("tests/camera_scene.json",w,settings));
    check(near(w.GetComponent<Camera>(settings.activeCamera).fovYDegrees,60));
    context.activeCamera=settings.activeCamera;
    transform.Update(w,commands,0); camera.Update(w,commands,0); check(context.ready);
    Logger::ShutDown(); std::cout << "Camera checks passed\n";
}
