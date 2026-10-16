#ifndef TRANSFORM_SYSTEM_HPP
#define TRANSFORM_SYSTEM_HPP

#include "ecs/System.hpp"
#include "ecs/Entity.hpp"

#include <cstdint>
#include <vector>

// Считает WorldTransform для всех сущностей с Transform + WorldTransform.
// Для job-системы: все сущности одной глубины независимы друг от друга,
// поэтому каждый слой глубины можно делить на куски и считать параллельно.
class TransformSystem final : public ISystem {
public:
	const char* GetName() const override { return "TransformSystem"; }
	UpdatePhase GetPhase() const override { return UpdatePhase::Transform; }
	SystemAccess GetAccess() const override;

	void Update(World& world, CommandBuffer& commands, float dt) override;
private:
	struct Item {
		std::uint32_t depth;
		Entity entity;
	};
	std::vector<Item> m_order;	// Переиспользуется между кадрами. без лишних аллокаций
};

#endif