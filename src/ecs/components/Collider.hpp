#ifndef COLLIDER_HPP
#define COLLIDER_HPP

#include <glm/glm.hpp>

enum class ColliderType {
	Box,
	Sphere
};

struct Collider {
	ColliderType type = ColliderType::Box;

	glm::vec3 halfExtents = glm::vec3(0.5f);
	glm::vec3 offset = glm::vec3(0.0f);
	bool isTrigger = false;
};

#endif