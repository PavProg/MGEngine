#ifndef HIERARCHY_HPP
#define HIERARCHY_HPP

#include "ecs/Entity.hpp"
#include <vector>

// менять ТОЛЬКО через HierarchyUtils, иначе parent и children разойдутся
struct Hierarchy {
    Entity parent = NULL_ENTITY;
    std::vector<Entity> children;
};

#endif