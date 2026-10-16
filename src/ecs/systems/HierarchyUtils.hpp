#ifndef HIERARCHY_UTILS_HPP
#define HIERARCHY_UTILS_HPP

#include "ecs/World.hpp"
#include "ecs/Entity.hpp"

// Пространство имен чтобы не загружать World
namespace HierarchyUtils {
	// parent = NULL_ENTITY - отвязать от родителя
	bool SetParent(World& world, Entity child, Entity parent);

	// Удалить сущность вместе со всеми потомками
	void DestroyWithChildren(World& world, Entity entity);
}

#endif