#include "CameraControllerSystem.hpp"

#include "ecs/World.hpp"
#include "ecs/components/FreeFlyCameraController.hpp"
#include "ecs/components/Hierarchy.hpp"
#include "ecs/components/Transform.hpp"
#include "input/InputFrame.hpp"

#include <cmath>
#include <glm/gtc/constants.hpp>

SystemAccess CameraControllerSystem::GetAccess() const {
    SystemAccess access;
    access.Read<FreeFlyCameraController>().Read<Hierarchy>().Write<Transform>();
    return access;
}

void CameraControllerSystem::Update(World& world, CommandBuffer&, float dt) {
    const bool sameCamera = m_previousCamera == m_activeCamera;
    m_previousCamera = m_activeCamera;
    if (!m_input.focused || !m_input.cursorCaptured || !world.IsAlive(m_activeCamera)) return;

    auto* transform = world.TryGetComponent<Transform>(m_activeCamera);
    const auto* controller = world.TryGetComponent<FreeFlyCameraController>(m_activeCamera);
    const auto* hierarchy = world.TryGetComponent<Hierarchy>(m_activeCamera);
    if (!transform || !controller || !controller->enabled ||
        (hierarchy && !hierarchy->parent.IsNull()) || transform->scale != glm::vec3(1.0f)) return;

    const float rotationLengthSquared = glm::dot(transform->rotation, transform->rotation);
    if (!std::isfinite(rotationLengthSquared) || rotationLengthSquared <= 0.0f) return;

    // Re-read the pose so switching cameras or editing Transform cannot leave stale angles.
    const glm::quat rotation = glm::normalize(transform->rotation);
    const glm::vec3 right = rotation * glm::vec3(1.0f, 0.0f, 0.0f);
    float yaw = std::atan2(-right.z, right.x);
    const glm::vec3 up(0.0f, 1.0f, 0.0f);
    if (sameCamera && m_input.mouseDelta != glm::dvec2(0.0) &&
        std::isfinite(m_input.mouseDelta.x) && std::isfinite(m_input.mouseDelta.y) &&
        std::isfinite(controller->lookSensitivityDegrees) && controller->lookSensitivityDegrees > 0.0f) {
        const double sensitivity = glm::radians(static_cast<double>(controller->lookSensitivityDegrees));
        yaw = static_cast<float>(std::remainder(yaw - m_input.mouseDelta.x * sensitivity, glm::two_pi<double>()));
        const glm::vec3 forward = rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        const double pitch = std::asin(glm::clamp(forward.y, -1.0f, 1.0f));
        const double limit = glm::radians(89.0);
        const float nextPitch = static_cast<float>(glm::clamp(pitch - m_input.mouseDelta.y * sensitivity, -limit, limit));
        transform->rotation = glm::angleAxis(yaw, up) * glm::angleAxis(nextPitch, glm::vec3(1.0f, 0.0f, 0.0f));
    }

    const glm::vec3 direction(
        int(m_input.IsDown(InputKey::D)) - int(m_input.IsDown(InputKey::A)),
        int(m_input.IsDown(InputKey::E)) - int(m_input.IsDown(InputKey::Q)),
        int(m_input.IsDown(InputKey::S)) - int(m_input.IsDown(InputKey::W))
    );
    if (direction == glm::vec3(0.0f) || !std::isfinite(dt) || dt <= 0.0f ||
        !std::isfinite(controller->moveSpeed) || controller->moveSpeed <= 0.0f) return;

    const bool sprint = m_input.IsDown(InputKey::LeftShift) || m_input.IsDown(InputKey::RightShift);
    const float distance = controller->moveSpeed * (sprint ? 3.0f : 1.0f) * dt;
    transform->position += (glm::angleAxis(yaw, up) * glm::normalize(direction)) * distance;
}
