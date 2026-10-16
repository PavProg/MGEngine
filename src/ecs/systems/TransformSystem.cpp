#include "TransformSystem.hpp"
#include "ecs/World.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/components/WorldTransform.hpp"
#include "ecs/components/Hierarchy.hpp"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

static constexpr std::uint32_t MAX_HIERARCHY_DEPTH = 32;

static glm::mat4 BuildLocalMatrix(const Transform& t) {
	glm::mat4 m = glm::translate(glm::mat4(1.0f), t.position);
	m = m * glm::mat4_cast(t.rotation);
	m = glm::scale(m, t.scale);
	return m;
}

static Entity GetParent(World& world, Entity entity) {
	const Hierarchy* h = world.TryGetComponent<Hierarchy>(entity);
	return h ? h->parent : NULL_ENTITY;
}

// Глубина = сколько родителей под сущностью
static std::uint32_t ComputeDepth(World& world, Entity entity) {
	std::uint32_t depth = 0;
	Entity parent = GetParent(world, entity);
	while (world.IsAlive(parent) && depth < MAX_HIERARCHY_DEPTH) {
		++depth;
		parent = GetParent(world, parent);
	}
	return depth;
}

SystemAccess TransformSystem::GetAccess() const {
	SystemAccess access;
	access.Read<Transform>().Read<Hierarchy>().Write<WorldTransform>();
	return access;
}

void TransformSystem::Update(World& world, CommandBuffer&, float) {
	// собираем сущности и их глубину
	m_order.clear();
	world.GetView<const Transform, WorldTransform>().Each(
		[&](Entity entity, const Transform&, WorldTransform&) {
			m_order.push_back({ ComputeDepth(world, entity), entity });
		});

	// Родители раньше детей
	std::sort(m_order.begin(), m_order.end(), [](const Item& a, const Item& b) {
		return a.depth < b.depth;
	});

	// матрицы: мировая = мировая родителя * локальная
	auto& transforms = world.GetPool<Transform>();
	auto& worldTransforms = world.GetPool<WorldTransform>();

	for (const Item& item : m_order) {
		glm::mat4 local = BuildLocalMatrix(transforms.Get(item.entity));

		const Entity parent = GetParent(world, item.entity);
		const WorldTransform* parentWorld = worldTransforms.TryGet(parent);

		worldTransforms.Get(item.entity).matrix =
			parentWorld ? parentWorld->matrix * local : local;
	}
}