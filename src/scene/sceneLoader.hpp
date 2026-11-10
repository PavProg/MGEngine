#ifndef SCENE_LOADER_HPP
#define SCENE_LOADER_HPP

#include <string>

#include "ecs/World.hpp"
#include "scene/SceneSettings.hpp"

class ResourceManager;

namespace SceneLoader {
	bool Load(const std::string& path, World& world, SceneSettings& settings,
		ResourceManager* resources = nullptr);
}

#endif
