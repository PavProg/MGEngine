#ifndef RENDERSYSTEM_HPP
#define RENDERSYSTEM_HPP

#include "ecs/System.hpp"
#include "render/IRenderAdapter.hpp"

// Рисует все сущности с WorldTransform + MeshRenderer.
// Матрицы уже посчитаны TransformSystem - здесь только чтение
class RenderSystem final : public ISystem {
public:
	explicit RenderSystem(IRenderAdapter& renderer) : m_renderer(renderer) {}

	const char* GetName() const override { return "RenderSystem"; }
	UpdatePhase GetPhase() const override { return UpdatePhase::Render; }
	SystemAccess GetAccess() const override;

	void Update(World& world, CommandBuffer& commands, float dt) override;
private:
	IRenderAdapter& m_renderer;
};

#endif