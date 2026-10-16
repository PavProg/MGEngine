#include "sceneLoader.hpp"
#include "componentRegistry.hpp"

#include <fstream>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "ecs/systems/HierarchyUtils.hpp"
#include "logger/logger.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

using nlohmann::json;

namespace {
	glm::vec3 ReadVec3(const json& arr, const glm::vec3& def) {
		if (!arr.is_array() || arr.size() < 3) return def;
		return glm::vec3(arr[0].get<float>(), arr[1].get<float>(), arr[2].get<float>());
	}

	glm::vec4 ReadVec4(const json& arr, const glm::vec4& def) {
		if (!arr.is_array() || arr.size() < 4) return def;
		return glm::vec4(
			arr[0].get<float>(), arr[1].get<float>(),
			arr[2].get<float>(), arr[3].get<float>()
		);
	}

	void ParseSceneSettings(const json& j, SceneSettings& s) {
		if (j.contains("background"))
			s.background = ReadVec4(j["background"], s.background);

		if (j.contains("camera")) {
			const json& c = j["camera"];

			if (c.contains("projection")) {
				std::string p = c["projection"].get<std::string>();
				s.camera.projection = (p == "perspective")
					? ProjectionType::Perspective
					: ProjectionType::Orthographic;
			}

			if (c.contains("position")) s.camera.position = ReadVec3(c["position"], s.camera.position);
			if (c.contains("target")) s.camera.target = ReadVec3(c["target"], s.camera.target);
			if (c.contains("up")) s.camera.up = ReadVec3(c["up"], s.camera.up);
			if (c.contains("fov")) s.camera.fov = c["fov"].get<float>();
			if (c.contains("orthoSize")) s.camera.orthoSize = c["orthoSize"].get<float>();
			if (c.contains("near")) s.camera.nearPlan = c["near"].get<float>();
			if (c.contains("far")) s.camera.farPlan = c["far"].get<float>();
		}
	}
}

namespace SceneLoader {
	bool Load(const std::string& path, World& world, SceneSettings& settings) {
		// Открыть файл
		std::ifstream file(path);
		if (!file.is_open()) {
			Logger::Error("SceneLoader: cannot open file " + path);
			return false;
		}

		// Распарсить json
		json root;
		try {
			file >> root;
		}
		catch (const std::exception& e) {
			Logger::Error("SceneLoader: JSON parse error: " + std::string(e.what()));
			return false;
		}

		// Проверить версию формата
		int version = root.value("version", 1);
		if (version != 1) {
			Logger::Warning("SceneLoader: unsupported scene version "
				+ std::to_string(version));
		}

		// Считать настройки сцены
		if (root.contains("sceneSettings"))
			ParseSceneSettings(root["sceneSettings"], settings);

		// Подготовить реестр
		const ComponentRegistry registry = ComponentRegistry::CreateDefault();

		if (!root.contains("entities") || !root["entities"].is_array()) {
			Logger::Warning("SceneLoader: no entities array in scene");
			return true;
		}

		// 1. Создание сущностей и компонентов
		std::unordered_map<std::string, Entity> idMap;

		for (const auto& entJson : root["entities"]) {
			std::string id = entJson.value("id", std::string{});
			Entity e = world.CreateEntity();
			if (!id.empty()) idMap[id] = e;

			if (entJson.contains("components") && entJson["components"].is_object()) {
				const json& comps = entJson["components"];
				for (auto it = comps.begin(); it != comps.end(); ++it) {
					registry.Load(it.key(), world, e, it.value());
				}
			}
		}

		// Расстановка parent->child связей
		for (const auto& entJson : root["entities"]) {
			if (!entJson.contains("parent")) continue;

			std::string childId = entJson.value("id", std::string{});
			std::string parentId = entJson["parent"].get<std::string>();

			auto childIt = idMap.find(childId);
			auto parentIt = idMap.find(parentId);

			if (childIt == idMap.end() || parentIt == idMap.end()) {
				Logger::Warning("SceneLoader: bad parent link "
					+ parentId + " for " + childId);
				continue;
			}
			HierarchyUtils::SetParent(world, childIt->second, parentIt->second);
		}

		Logger::Info("SceneLoader: scene loaded from " + path);
		return true;
	}
}