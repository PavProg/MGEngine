#ifndef CAMERA_CONTROLLER_SYSTEM_HPP
#define CAMERA_CONTROLLER_SYSTEM_HPP

#include "ecs/Entity.hpp"
#include "ecs/System.hpp"

struct InputFrame;

class CameraControllerSystem final : public ISystem {
public:
    // Application owns both references. Pass CameraContext::activeCamera here.
    CameraControllerSystem(const InputFrame& input, const Entity& activeCamera)
        : m_input(input), m_activeCamera(activeCamera) {}

    const char* GetName() const override { return "CameraControllerSystem"; }
    UpdatePhase GetPhase() const override { return UpdatePhase::Input; }
    SystemAccess GetAccess() const override;
    void Update(World& world, CommandBuffer& commands, float dt) override;

private:
    const InputFrame& m_input;
    const Entity& m_activeCamera;
    Entity m_previousCamera = NULL_ENTITY;
};

#endif
