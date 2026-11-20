#include "sceneLoader.hpp"
#include "componentRegistry.hpp"

#include <fstream>
#include <stdexcept>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "ecs/systems/HierarchyUtils.hpp"
#include "logger/logger.hpp"
#include "ecs/components/Camera.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/components/WorldTransform.hpp"

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
	bool Load(const std::string& path, World& world, SceneSettings& settings, ResourceManager* resources, bool importLegacyCamera) {
		std::ifstream file(path);

		if (!file.is_open())
		{
			Logger::Error("SceneLoader: cannot open file " + path);
			return false;
		}

		try {
			json root;
			file >> root;

			const int version = root.value("version", 1);

			if (version != 1 && version != 2)
			{
				Logger::Error("SceneLoader: unsupported scene version");
				return false;
			}

			settings = SceneSettings{};

			if (root.contains("sceneSettings")) {
				ParseSceneSettings(root["sceneSettings"], settings);
			}

			const ComponentRegistry resgistry = ComponentRegistry::CreateDefault(resources);
			if (!root.contains("entities")
				|| !root["entities"].is_array()) {
				Logger::Error("SceneLoader: entities array is required");
				return false;
			}

			std::unordered_map<std::string, Entity> idMap; // временная таблица, связывающая строковые имена из JSON с настоящими Entity

			// Первый проход создает сущности и компоненты
			for (const auto& entityData : root["entities"])
			{
				const std::string id = entityData.value("id", std::string{});

				if (!id.empty() && idMap.find(id) != idMap.end()) {
					Logger::Error("SceneLoader: duplicate entity id " + id);
					return false;
				}
				const Entity entity = world.CreateEntity();

				if (!id.empty()) {
					idMap.emplace(id, entity);
				}
				if (!entityData.contains("components")) {
					continue;
				}

				const json& components = entityData["components"];
				if (!components.is_object()) {
					Logger::Error("SceneLoader: components must be an object");
					return false;
				}

				for (auto it = components.begin(); it != components.end(); ++it) {
					if (!resgistry.Load(it.key(), world, entity, it.value())) {
						return false;
					}
				}
			}

			// Второй проход устанавливает родительские связи
			for (const auto& entityData : root["entities"]) {
				if (!entityData.contains("parent")) {
					continue;
				}

				const std::string childId = entityData.value("id", std::string{});
				const std::string parentId = entityData["parent"].get<std::string>();
				const auto child = idMap.find(childId);
				const auto parent = idMap.find(parentId);

				if (child == idMap.end() || parent == idMap.end()) {
					Logger::Error("SceneLoader: invalid parent link");
					return false;
				}

				if (!HierarchyUtils::SetParent(world, child->second, parent->second)) {
					Logger::Error("SceneLoader: parent link rejected");
					return false;
				}
			}

			// Выбор активной камеры после создания всех сущностей
			const json scene = root.value("sceneSettings", json::object());

			if (scene.contains("activeCamera")) {
				const std::string id = scene.at("activeCamera").get<std::string>();
				const auto found = idMap.find(id);

				if (found == idMap.end()
					|| !world.HasComponent<Camera>(found->second)
					|| !world.HasComponent<Transform>(found->second)
					|| !world.HasComponent<WorldTransform>(found->second)) {
					Logger::Error("SceneLoader: invalid activeCamera");
					return false;
				}
				settings.activeCamera = found->second;
			}
			else if (version == 1) {
				// Для теста трансформов камера не создается
				if (importLegacyCamera) {
					const CameraSettings& old = settings.camera;

					Camera camera{};
					camera.projection = old.projection;
					camera.fovYDegrees = old.fov;
					camera.nearPlane = old.nearPlan;
					camera.farPlane = old.farPlan;
					camera.orthoSize = old.orthoSize;

					if (!camera.IsValid()) {
						Logger::Error("SceneLoader: invalid legacy camera");
						return false;
					}

					Transform transform{};
					transform.position = old.position;
					transform.rotation = glm::quat_cast(glm::inverse(glm::lookAt(old.position, old.target, old.up)));
					const Entity entity = world.CreateEntity();
					world.AddComponent<Camera>(entity, camera);
					world.AddComponent<Transform>(entity, transform);
					world.AddComponent<WorldTransform>(entity, WorldTransform{});
					settings.activeCamera = entity;
				}
			}
			else {
				Logger::Error("SceneLoader: activeCamera is required");
				return false;
			}

			Logger::Info("SceneLoader: scene loaded from " + path);
			return true;
		}
		catch (const std::exception& error) {
			Logger::Error("SceneLoader: " + path + ": " + error.what());
			return false;
		}
	}
}