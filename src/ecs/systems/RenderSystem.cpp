#include "RenderSystem.hpp"
#include "ecs/World.hpp"
#include "ecs/components/MeshRenderer.hpp"
#include "ecs/components/WorldTransform.hpp"

SystemAccess RenderSystem::GetAccess() const {
	SystemAccess access;
	access.Read<WorldTransform>().Read<MeshRenderer>();
	return access;
}

void RenderSystem::Update(World& world, CommandBuffer&, float) {
    world.GetView<const WorldTransform, const MeshRenderer>().Each(
        [&](Entity, const WorldTransform& wt, const MeshRenderer& mr) {
            if (mr.mesh && mr.shader && mr.texture) {
                m_renderer.DrawLoadedMesh(
                    mr.mesh->data.gpu,
                    mr.texture->data.gpu,
                    mr.shader->data.gpu,
                    wt.matrix,
                    mr.tint
                );
            }
            else {
                m_renderer.DrawMesh(wt.matrix, mr.primitiveType, mr.color);
            }
        });
}