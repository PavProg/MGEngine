#ifndef RIGIDBODY_HPP
#define RIGIDBODY_HPP

#include <glm/glm.hpp>

struct Rigidbody {
	glm::vec3 velocity = glm::vec3(0.0f);
	glm::vec3 acceleration = glm::vec3(0.0f);
	float mass = 1.0f;

	bool useGravity = false;
	bool isStatic = false;	// true - неподвижный
};

#endif