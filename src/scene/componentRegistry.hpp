#ifndef COMPONENT_REGISTRY_HPP
#define COMPONENT_REGISTRY_HPP

#include <unordered_map>
#include <string>
#include <functional>

#include <nlohmann/json.hpp>

#include "ecs/World.hpp"

class ComponentRegistry {
public:
	using Loader = std::function<void(World&, Entity, const nlohmann::json&)>;

	void Register(const std::string& name, Loader loader);

	bool Load(const std::string& name, World& world, Entity entity, const nlohmann::json& data) const;

	static ComponentRegistry CreateDefault();
private:
	std::unordered_map<std::string, Loader> m_loaders;
};

#endif