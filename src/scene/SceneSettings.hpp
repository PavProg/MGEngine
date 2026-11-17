#ifndef SCENE_SETTINGS_HPP
#define SCENE_SETTINGS_HPP

#include <glm/glm.hpp>
#include "ecs/Entity.hpp"
#include "ecs/components/Camera.hpp"



struct CameraSettings {
	ProjectionType projection = ProjectionType::Orthographic;

	glm::vec3 position = glm::vec3(0.0f, 0.0f, 20.0f);
	glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
	float nearPlan = 0.1f;
	float farPlan = 200.0f;

	float fov = 45.0f;

	// Ортографическая камера
	float orthoSize = 10.0f;
};

struct SceneSettings {
	glm::vec4 background = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	CameraSettings camera; // Legacy version 1 import only
	Entity activeCamera = NULL_ENTITY;
};

#endif