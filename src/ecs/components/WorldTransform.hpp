#ifndef WORLD_TRANSFORM_HPP
#define WORLD_TRANSFORM_HPP

#include <glm/glm.hpp>

// Итоговая матрица объекта в мире.
// Пишет только TransformSystem, все остальные только читают.
// Transform при этом - положение относительно родителя.
struct WorldTransform {
	glm::mat4 matrix = glm::mat4(1.0f);
};

#endif