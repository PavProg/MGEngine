#include "PhysicsSystem.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/components/Rigidbody.hpp"
#include "ecs/components/Collider.hpp"
#include "logger/logger.hpp"

#include <cmath>
#include <vector>

PhysicsSystem::PhysicsSystem(World& world) : m_world(world) {}

void PhysicsSystem::Update(float dt) {
	Integrate(dt);
	ResolveCollisions();
}

void PhysicsSystem::Integrate(float dt) {
	const auto& bodies = m_world.GetAllComponents<Rigidbody>();
	for (const auto& entry : bodies) {
		EntityId e = entry.first;
		Rigidbody& rb = m_world.GetComponent<Rigidbody>(e);

		if (rb.isStatic) continue;
		if (!m_world.HasComponent<Transform>(e)) continue;

		Transform& t = m_world.GetComponent<Transform>(e);

		glm::vec3 accel = rb.acceleration;
		if (rb.useGravity) accel.y -= m_gravity;

		rb.velocity += accel * dt;
		t.position += rb.velocity * dt;
	}
}

bool PhysicsSystem::ComputeAABB(EntityId e, AABB& out) {
	if (!m_world.HasComponent<Transform>(e)) return false;
	if (!m_world.HasComponent<Collider>(e)) return false;

	Transform& t = m_world.GetComponent<Transform>(e);
	Collider& c = m_world.GetComponent<Collider>(e);

	out.center = glm::vec2(t.position.x + c.offset.x * t.scale.x, t.position.y + c.offset.y * t.scale.y);
	out.halfExtents = glm::vec2(c.halfExtents.x * t.scale.x, c.halfExtents.y * t.scale.y);

	return true;
}

void PhysicsSystem::ResolveCollisions() {
	const auto& colliders = m_world.GetAllComponents<Collider>();
	std::vector<EntityId> ents;
	ents.reserve(colliders.size());
	for (const auto& entry : colliders) ents.push_back(entry.first);

	for (size_t i = 0; i < ents.size(); ++i) {
		for (size_t j = i + 1; j < ents.size(); ++j) {
			EntityId eA = ents[i];
			EntityId eB = ents[j];

			bool movA = m_world.HasComponent<Rigidbody>(eA) && !m_world.GetComponent<Rigidbody>(eA).isStatic;
			bool movB = m_world.HasComponent<Rigidbody>(eB) && !m_world.GetComponent<Rigidbody>(eB).isStatic;

			if (!movA && !movB) continue;

			AABB a, b;
			if (!ComputeAABB(eA, a)) continue;
			if (!ComputeAABB(eB, b)) continue;

			float dx = a.center.x - b.center.x;
			float dy = a.center.y - b.center.y;
			float overlapX = (a.halfExtents.x + b.halfExtents.x) - std::abs(dx);	// std::abs - модуль
			float overlapY = (a.halfExtents.y + b.halfExtents.y) - std::abs(dy);

			if (overlapX <= 0.0f || overlapY <= 0.0f) continue;

			bool trigger = m_world.GetComponent<Collider>(eA).isTrigger || m_world.GetComponent<Collider>(eB).isTrigger;
			if (trigger) continue;

			glm::vec2 correction(0.0f);
			if (overlapX < overlapY) {
				correction.x = (dx < 0.0f ? -overlapX : overlapX);
			}
			else {
				correction.y = (dy < 0.0f ? -overlapY : overlapY);
			}

			Transform& tA = m_world.GetComponent<Transform>(eA);
			Transform& tB = m_world.GetComponent<Transform>(eB);

			if (movA && movB) {
				tA.position.x += correction.x * 0.5f;
				tA.position.y += correction.y * 0.5f;
				tB.position.x += correction.x * 0.5f;
				tB.position.y -= correction.y * 0.5f;
			}
			else if (movA) {
				tA.position.x += correction.x * 0.5f;
				tA.position.y += correction.y * 0.5f;
			}
			else {
				tB.position.x += correction.x * 0.5f;
				tB.position.y += correction.y * 0.5f;
			}

			if (correction.x != 0.0f) {
				if (movA) m_world.GetComponent<Rigidbody>(eA).velocity.x = 0.0f;
				if (movB) m_world.GetComponent<Rigidbody>(eB).velocity.x = 0.0f;
			}
			if (correction.y != 0.0f) {
				if (movA) m_world.GetComponent<Rigidbody>(eA).velocity.y = 0.0f;
				if (movB) m_world.GetComponent<Rigidbody>(eB).velocity.y = 0.0f;
			}
		}
	}
}