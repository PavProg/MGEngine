#include "CameraSystem.hpp"
#include "ecs/World.hpp"
#include "ecs/components/Camera.hpp"
#include "ecs/components/WorldTransform.hpp"
#include "render/IRenderAdapter.hpp"
#include <glm/gtc/matrix_transform.hpp>
SystemAccess CameraSystem::GetAccess() const {
    SystemAccess access;
    return access.Read<Camera>().Read<WorldTransform>();
}
void CameraSystem::Update(World& world, CommandBuffer&, float) {
    m_context.ready = false;
    if (m_context.framebufferWidth <= 0 || m_context.framebufferHeight <= 0 || !world.IsAlive(m_context.activeCamera)) return;
    const auto* camera = world.TryGetComponent<Camera>(m_context.activeCamera);
    const auto* pose = world.TryGetComponent<WorldTransform>(m_context.activeCamera);
    if (!camera || !pose || !camera->IsValid()) return;
    const float determinant = glm::determinant(pose->matrix);
    if (!std::isfinite(determinant) || std::abs(determinant) < 1e-8f) return;
    const glm::mat4 view = glm::inverse(pose->matrix);
    for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r)
        if (!std::isfinite(view[c][r])) return;
    const float aspect = float(m_context.framebufferWidth) / float(m_context.framebufferHeight);
    const float h = camera->orthoSize;
    const glm::mat4 projection = camera->projection == ProjectionType::Orthographic
        ? glm::ortho(-h * aspect, h * aspect, -h, h, camera->nearPlane, camera->farPlane)
        : glm::perspective(glm::radians(camera->fovYDegrees), aspect, camera->nearPlane, camera->farPlane);
    m_renderer.SetCamera(view, projection);
    m_context.ready = true;
}
