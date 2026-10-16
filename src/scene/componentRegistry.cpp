#include "componentRegistry.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "ecs/components/Transform.hpp"
#include "ecs/components/WorldTransform.hpp"
#include "ecs/components/Tag.hpp"
#include "ecs/components/MeshRenderer.hpp"
#include "ecs/components/Hierarchy.hpp"
#include "logger/logger.hpp"

using nlohmann::json;

namespace {
	glm::vec3 ReadVec3(const json& arr, const glm::vec3& def = glm::vec3(0.0f)) {
		if (!arr.is_array() || arr.size() < 3) return def;
		return glm::vec3(
			arr[0].get<float>(),
			arr[1].get<float>(),
			arr[2].get<float>()
		);
	}

	glm::vec4 ReadVec4(const json& arr, const glm::vec4& def = glm::vec4(1.0f)) {
		if (!arr.is_array() || arr.size() < 4) return def;
		return glm::vec4(
			arr[0].get<float>(), arr[1].get<float>(),
			arr[2].get<float>(), arr[3].get<float>()
		);
	}

	glm::quat ReadQuat(const json& arr, const glm::quat& def = glm::quat(1.0f, 0.0f, 0.0f, 0.0f)) {
		if (!arr.is_array() || arr.size() < 4) return def;
		return glm::quat(
			arr[0].get<float>(),
			arr[1].get<float>(),
			arr[2].get<float>(),
			arr[3].get<float>()
		);
	}

	void LoadTransform(World& world, Entity e, const json& j) {
		Transform t{};
		if (j.contains("position")) t.position = ReadVec3(j["position"], t.position);
		if (j.contains("rotation")) t.rotation = ReadQuat(j["rotation"], t.rotation);
		if (j.contains("scale")) t.scale = ReadVec3(j["scale"], t.scale);
		world.AddComponent<Transform>(e, t);
		// Мировую матрицу посчитает TransformSystem - добавляем всегда вместе с Transform
		world.AddComponent<WorldTransform>(e, WorldTransform{});
	}

	void LoadTag(World& world, Entity e, const json& j) {
		Tag t{};
		if (j.contains("name")) t.name = j["name"].get<std::string>();
		world.AddComponent<Tag>(e, t);
	}

	PrimitiveType ParsePrimitive(const std::string& s) {
		if (s == "Triangle") return PrimitiveType::Triangle;
		if (s == "Square") return PrimitiveType::Square;
		if (s == "Cube") return PrimitiveType::Cube;
		Logger::Warning("Unknown primitive \"" + s + "\", falling back to Triangle");
		return PrimitiveType::Triangle;
	}

	void LoadMeshRenderer(World& world, Entity e, const json& j) {
		MeshRenderer m{};
		if (j.contains("primitive")) 
			m.primitiveType = ParsePrimitive(j["primitive"].get<std::string>());
		if (j.contains("color")) 
			m.color = ReadVec4(j["color"], m.color);
		world.AddComponent<MeshRenderer>(e, m);
	}

	void LoadHierarchy(World& world, Entity e, const json&) {
		if (!world.HasComponent<Hierarchy>(e))
			world.AddComponent<Hierarchy>(e, Hierarchy{});
	}
}

// Реализация методов класса

void ComponentRegistry::Register(const std::string& name, Loader loader) {
	m_loaders[name] = std::move(loader);
}

bool ComponentRegistry::Load(const std::string& name, World& world, Entity entity, const json& data) const {
	auto it = m_loaders.find(name);
	if (it == m_loaders.end()) {
		Logger::Warning("Unknown component type in scene: " + name);
		return false;
	}
	try {
		it->second(world, entity, data);
		return true;
	}
	catch (const std::exception& e) {
		Logger::Error("Failed to load component " + name + ": " + e.what());
		return false;
	}
}

ComponentRegistry ComponentRegistry::CreateDefault() {
	ComponentRegistry r;
	r.Register("Transform", LoadTransform);
	r.Register("Tag", LoadTag);
	r.Register("MeshRenderer", LoadMeshRenderer);
	r.Register("Hierarchy", LoadHierarchy);
	return r;
}