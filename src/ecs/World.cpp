#include "World.hpp"

Entity World::CreateEntity() {
	assert(!m_structureLocked && "CreateEntity inside a system: use CommandBuffer");

	std::uint32_t index;
	if (!m_freeIndices.empty()) {
		index = m_freeIndices.back();
		m_freeIndices.pop_back();
	}
	else {
		index = static_cast<std::uint32_t>(m_generations.size());
		m_generations.push_back(1);
	}

	++m_aliveCount;
	return Entity{ index, m_generations[index] };
}

void World::DestroyEntity(Entity entity) {
	assert(!m_structureLocked && "DestroyEntity inside a system: use CommandBuffer");
	if (!IsAlive(entity)) return;

	for (auto& pool : m_pools) {
		if (pool) pool->Remove(entity);
	}

	std::uint32_t& generation = m_generations[entity.index];
	++generation;
	if (generation == 0) generation = 1;

	m_freeIndices.push_back(entity.index);
	--m_aliveCount;
}

bool World::IsAlive(Entity entity) const {
	return !entity.IsNull()
		&& entity.index < m_generations.size()
		&& m_generations[entity.index] == entity.generation;
}