#pragma once
#include "ecs/System.hpp"
#include "ecs/Entity.hpp"
class IRenderAdapter;
struct CameraContext {
    Entity activeCamera = NULL_ENTITY;
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    bool ready = false;
};
class CameraSystem final : public ISystem {
public:
    CameraSystem(IRenderAdapter& renderer, CameraContext& context) : m_renderer(renderer), m_context(context) {}
    const char* GetName() const override { return "CameraSystem"; }
    UpdatePhase GetPhase() const override { return UpdatePhase::Camera; }
    SystemAccess GetAccess() const override;
    void Update(World&, CommandBuffer&, float) override;
private:
    IRenderAdapter& m_renderer;
    CameraContext& m_context;
};
