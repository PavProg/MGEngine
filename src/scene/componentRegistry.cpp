#include "componentRegistry.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "ecs/components/Camera.hpp"
#include "ecs/components/FreeFlyCameraController.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/components/WorldTransform.hpp"
#include "ecs/components/Tag.hpp"
#include "ecs/components/MeshRenderer.hpp"
#include "ecs/components/Hierarchy.hpp"
#include "logger/logger.hpp"
#include "resources/ResourceManager.hpp"

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

	void LoadCamera(World& world, Entity entity, const json& data)
	{
		Camera camera{};

		const std::string projection = data.value("projection", std::string("perspective"));

		if (projection != "perspective" && projection != "orthographic")
		{
			throw std::invalid_argument("Invalid camera projection");
		}

		camera.projection = projection == "perspective" ? ProjectionType::Perspective : ProjectionType::Orthographic;

		camera.fovYDegrees = data.value(
			"fovYDegrees",
			data.value("fov", camera.fovYDegrees)
		);

		camera.nearPlane = data.value("near", camera.nearPlane);
		camera.farPlane = data.value("far", camera.farPlane);
		camera.orthoSize = data.value("orthoSize", camera.orthoSize);

		if (!camera.IsValid())
		{
			throw std::invalid_argument("Invalid camera frustum");
		}

		world.AddComponent<Camera>(entity, camera);
	}

	void LoadFreeFlyCameraController(World& world, Entity entity, const json& data)
	{
		FreeFlyCameraController controller{};

		controller.enabled = data.value("enabled", controller.enabled);
		controller.moveSpeed = data.value("moveSpeed", controller.moveSpeed);

		controller.lookSensitivityDegrees = data.value("lookSensitivityDegrees", controller.lookSensitivityDegrees);

		if (!std::isfinite(controller.moveSpeed)
			|| controller.moveSpeed < 0.0f
			|| !std::isfinite(controller.lookSensitivityDegrees)
			|| controller.lookSensitivityDegrees < 0.0f
			) {
			throw std::invalid_argument("Invalid camera speed or sensitivity");
		}

		world.AddComponent<FreeFlyCameraController>(entity, controller);
	}

	PrimitiveType ParsePrimitive(const std::string& s) {
		if (s == "Triangle") return PrimitiveType::Triangle;
		if (s == "Square") return PrimitiveType::Square;
		if (s == "Cube") return PrimitiveType::Cube;
		Logger::Warning("Unknown primitive \"" + s + "\", falling back to Triangle");
		return PrimitiveType::Triangle;
	}

	void LoadMeshRenderer(World& world, Entity entity, const json& data, ResourceManager* resources)
	{
		MeshRenderer renderer{};

		const bool hasModelResources = data.contains("mesh") || data.contains("texture") || data.contains("shader");

		if (hasModelResources)
		{
			if (data.contains("primitive"))
			{
				throw std::invalid_argument("MeshRenderer specifies both primitive and model");
			}

			if (!resources)
			{
				throw std::invalid_argument("Model loading requires ResourceManager");
			}

			const std::string mesh = data.at("mesh").get<std::string>();
			const std::string texture = data.at("texture").get<std::string>();
			const std::string shader = data.at("shader").get<std::string>();

			if (mesh.empty() || texture.empty() || shader.empty())
			{
				throw std::invalid_argument("MeshRenderer requires mesh, texture and shader");
			}

			for (const auto* path : { &mesh, &texture })
			{
				std::error_code err;

				if (!std::filesystem::is_regular_file(*path, err))
				{
					Logger::Warning("MeshRenderer: using placeholder for " + *path);
				}
			}

			renderer.primitiveType = PrimitiveType::None;
			renderer.mesh = resources->LoadAsync<Mesh>(mesh);
			renderer.texture = resources->LoadAsync<Texture>(texture);
			renderer.shader = resources->Load<ShaderProgram>(shader); // TODO

			if (!renderer.shader)
			{
				throw std::runtime_error("Cannot load model shader: " + shader);
			}

			if (data.contains("tint"))
			{
				renderer.tint = ReadVec4(data["tint"], renderer.tint);
			}
		}
		else {
			if (data.contains("primitive")) {
				renderer.primitiveType = ParsePrimitive(data["primitive"].get<std::string>());
			}

			if (data.contains("color")) {
				renderer.color = ReadVec4(data["color"], renderer.color);
			}
		}

		world.AddComponent<MeshRenderer>(entity, renderer);
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

ComponentRegistry ComponentRegistry::CreateDefault(ResourceManager* resources) {
	ComponentRegistry r;

	r.Register("Transform", LoadTransform);
	r.Register("Tag", LoadTag);
	r.Register("Camera", LoadCamera);
	r.Register("FreeFlyCameraController", LoadFreeFlyCameraController);

	// лямбда сохранит указатель на менеджер ресурсов
	r.Register("MeshRenderer", [resources](World& world, Entity entity, const json& data) {
		LoadMeshRenderer(world, entity, data, resources); });

	r.Register("Hierarchy", LoadHierarchy);
	return r;
}