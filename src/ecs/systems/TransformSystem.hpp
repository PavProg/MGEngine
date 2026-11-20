#ifndef TRANSFORM_SYSTEM_HPP
#define TRANSFORM_SYSTEM_HPP

#include "ecs/System.hpp"
#include "ecs/Entity.hpp"

#include <cstddef>
#include <vector>

class JobSystem;

// Считает WorldTransform для всех сущностей с Transform + WorldTransform.
// Для job-системы: все сущности одной глубины независимы друг от друга,
// поэтому каждый слой глубины можно делить на куски и считать параллельно.
class TransformSystem final : public ISystem {
public:
	explicit TransformSystem(JobSystem& jobs)
		: m_jobs(jobs) { }

	const char* GetName() const override { return "TransformSystem"; }
	UpdatePhase GetPhase() const override { return UpdatePhase::Transform; }
	SystemAccess GetAccess() const override;

	bool RequiresMainThread() const override {
		return false;
	}

	void Update(World& world, CommandBuffer& commands, float dt) override;
private:
	struct Item {
		std::size_t depth;
		Entity entity;
		Entity parent;
	};

	JobSystem& m_jobs;
	std::vector<Item> m_order;	// Переиспользуется между кадрами. без лишних аллокаций
};

#endif