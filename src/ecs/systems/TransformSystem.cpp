#include "TransformSystem.hpp"

#include "core/JobSystem.hpp"
#include "core/profiler.hpp"
#include "ecs/World.hpp"
#include "ecs/components/Hierarchy.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/components/WorldTransform.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <stdexcept>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

// Считает WorldTransform для всех сущностей с Transform + WorldTransform.
// Для job-системы: все сущности одной глубины независимы друг от друга,
// поэтому каждый слой глубины можно делить на куски и считать параллельно.
namespace {
	constexpr std::uint32_t MIN_JOB_RANGE = 256;
	constexpr std::size_t MIN_PARALLEL_COUNT = 512;

	glm::mat4 BuildLocalMatrix(const Transform& transform) {
		glm::mat4 matrix = glm::translate(glm::mat4(1.0f), transform.position);

		matrix = matrix * glm::mat4_cast(transform.rotation);

		return glm::scale(matrix, transform.scale);
	}

	Entity GetParent(World& world, Entity entity) {
		const Hierarchy* hierarchy = world.TryGetComponent<Hierarchy>(entity);

		return hierarchy ? hierarchy->parent : NULL_ENTITY;
	}

	std::size_t ComputeDepth(World& world, Entity parent) {
		const std::size_t aliveCount = world.GetAliveCount();
		std::size_t depth = 0;

		while (world.IsAlive(parent)) {
			++depth;

			// Сама сущность тоже входит в aliveCount.
			// В цепочке без циклов у неё меньше aliveCount предков.
			if (depth >= aliveCount) {
				throw std::logic_error("TransformSystem: cycle in entity hierarchy");
			}

			parent = GetParent(world, parent);
		}
		return depth;
	}
}

SystemAccess TransformSystem::GetAccess() const {
	SystemAccess access;
	access.Read<Transform>()
		.Read<Hierarchy>()
		.Write<WorldTransform>();
	return access;
}

void TransformSystem::Update(World& world, CommandBuffer&, float) {
	MGE_PROFILE_SCOPE_N("TransformSystem");
	assert(world.IsStructureLocked());

	{
		MGE_PROFILE_SCOPE_N("TransformGather");
		m_order.clear();
		world.GetView<const Transform, const WorldTransform>().Each(
			[this, &world](Entity entity, const Transform&, const WorldTransform&) {
				const Entity parent = GetParent(world, entity);
				m_order.push_back(Item{ComputeDepth(world, parent), entity, parent});
			}
		);
	}

	{
		MGE_PROFILE_SCOPE_N("TransformSort");
		std::sort(m_order.begin(), m_order.end(), [](const Item& a, const Item& b) {
			return a.depth < b.depth;
		});
	}

	{
		MGE_PROFILE_SCOPE_N("TransformMatrices");
		const auto& order = m_order;
		const auto& transforms = world.GetPool<Transform>();
		auto& worldTransforms = world.GetPool<WorldTransform>();
		const bool canRunParallel = m_jobs.GetThreadCount() > 1;

		std::size_t first = 0;

		while (first < order.size()) {
			const std::size_t depth = order[first].depth;
			std::size_t last = first + 1;

			while (last < order.size() && order[last].depth == depth) {
				++last;
			}

			const std::size_t count = last - first;

			auto updateRange =
				[&order, &transforms, &worldTransforms, first](
					std::size_t begin,
					std::size_t end
					) {
						for (std::size_t offset = begin; offset < end; ++offset) {
							const Item& item = order[first + offset];

							const glm::mat4 local = BuildLocalMatrix(transforms.Get(item.entity));

							const WorldTransform* parentWorld = worldTransforms.TryGet(item.parent);

							WorldTransform& result = worldTransforms.Get(item.entity);

							result.matrix = parentWorld ? parentWorld->matrix * local : local;
						}
				};
			if (canRunParallel && count >= MIN_PARALLEL_COUNT &&
				count <= std::numeric_limits<std::uint32_t>::max()) {
				m_jobs.ParallelFor(
					static_cast<std::uint32_t>(count),
					MIN_JOB_RANGE,
					updateRange
				);
			}
			else {
				updateRange(0, count);
			}

			// ParallelFor возвращается после завершения всех диапозонов.
			// Теперь можно читать матрицы этого слоя из следующего
			first = last;
		}
	}
}
