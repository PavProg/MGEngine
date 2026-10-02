#ifndef PHYSICS_SYSTEM_HPP
#define PHYSICS_SYSTEM_HPP

#include <glm/glm.hpp>
#include "ecs/World.hpp"

struct AABB {
	glm::vec2 center;
	glm::vec2 halfExtents;
};

class PhysicsSystem {
public:
	explicit PhysicsSystem(World& world);

	void Update(float dt);

	void SetGravity(float g) { m_gravity = g; }
private:
	void Integrate(float dt);
	void ResolveCollisions();

	bool ComputeAABB(EntityId e, AABB& out);

	World& m_world;
	float m_gravity = 9.81f;
};

#endif