#ifndef RESOURCE_MANAGER_TPP
#define RESOURCE_MANAGER_TPP

#include "logger/logger.hpp"
#include "render/IRenderAdapter.hpp"
#include "MeshLoader.hpp"
#include "TextureLoader.hpp" 
#include "core/profiler.hpp"

// instantiation шаблона
template<typename T>
std::shared_ptr<Resource<T>> ResourceManager::Load(const std::string& path) {
	std::type_index typeKey = std::type_index(typeid(T));
	auto& typeCache = m_caches[typeKey];

	auto it = typeCache.find(path);
	if (it != typeCache.end()) {
		if (std::shared_ptr<void> alive = it->second.lock()) {
			return std::static_pointer_cast<Resource<T>>(alive);
		}
		typeCache.erase(it);
	}

	// Синхронная загрузка в вызывающем (обычно главном) потоке - для замеров фриза
	MGE_PROFILE_SCOPE_N("LoadSync");
	MGE_PROFILE_TEXT(path.c_str(), path.size());
	std::shared_ptr<Resource<T>> resource = LoadImpl<T>(path);
	if (!resource) {
		Logger::Error("ResourceManager: failed to load " + path);
		return nullptr;
	}

	typeCache[path] = std::weak_ptr<void>(
		std::static_pointer_cast<void>(resource)
	);

	Logger::Info("ResourceManager: loaded " + path);
	return resource;
}

template<typename T>
std::shared_ptr<Resource<T>> ResourceManager::LoadAsync(const std::string& path, JobPriority priority) {
	static_assert(std::is_same_v<T, Mesh> || std::is_same_v<T, Texture>,
		"LoadAsync поддерживает только Mesh и Texture");

	auto& typeCache = m_caches[std::type_index(typeid(T))];

	auto it = typeCache.find(path);
	if (it != typeCache.end()) {
		if (std::shared_ptr<void> alive = it->second.lock())
			return std::static_pointer_cast<Resource<T>>(alive);
		typeCache.erase(it);
	}

	// Копируем(!) значения
	const unsigned int phMeshVao = m_placeholderMesh.vao;
	const unsigned int phTexId = m_placeholderTex.textureId;
	IRenderAdapter& renderer = m_renderer;

	std::shared_ptr<Resource<T>> res(
		new Resource<T>{},
		[&renderer, phMeshVao, phTexId](Resource<T>* p) {
			if constexpr (std::is_same_v<T, Mesh>) {
				if (p->data.gpu.vao != 0 && p->data.gpu.vao != phMeshVao)
					renderer.ReleaseMesh(p->data.gpu);
			} else if constexpr (std::is_same_v<T, Texture>) {
				if (p->data.gpu.textureId != 0 && p->data.gpu.textureId != phTexId)
					renderer.ReleaseTexture(p->data.gpu);
			}
			delete p;
		}
	);

	res->path = path;
	res->state = ResourceState::Loading;

	if constexpr (std::is_same_v<T, Mesh>) res->data.gpu = m_placeholderMesh;
	else if constexpr (std::is_same_v<T, Texture>) res->data.gpu = m_placeholderTex;

	typeCache[path] = std::weak_ptr<void>(std::static_pointer_cast<void>(res));

	std::weak_ptr<Resource<T>> weakRes = res;
	++m_inFlight;	// загрузка "в полёте"; уменьшается первой строкой финализатора
	m_queue.Submit([this, path, weakRes]() {
		if constexpr (std::is_same_v<T, Mesh>) {
			auto data = std::make_shared<MeshData>();
			MGE_PROFILE_SCOPE_N("ParseMesh");
			MGE_PROFILE_TEXT(path.c_str(), path.size());
			bool ok = MeshLoader::ParseToData(path, *data);
			m_queue.PushFinalize([this, weakRes, data, ok]() {
				--m_inFlight;	// первой строкой: ниже есть ранние return
				auto r = weakRes.lock();
				if (!r) return;
				if (!ok) { r->state = ResourceState::Failed; return; }
				r->data.cpuData = std::move(*data);
				MGE_PROFILE_SCOPE_N("UploadMesh");
				r->data.gpu = m_renderer.UploadMesh(r->data.cpuData);
				std::error_code ec;
				r->lastModified = std::filesystem::last_write_time(r->path, ec);
				r->state = ResourceState::Ready;
				Logger::Info("Async: mesh ready " + r->path);
				});
		}
		else if constexpr (std::is_same_v<T, Texture>) {
			auto data = std::make_shared<TextureData>();
			MGE_PROFILE_SCOPE_N("ParseTexture");
			MGE_PROFILE_TEXT(path.c_str(), path.size());
			bool ok = TextureLoader::ParseToData(path, *data);
			m_queue.PushFinalize([this, weakRes, data, ok]() {
				--m_inFlight;	// первой строкой: ниже есть ранние return
				auto r = weakRes.lock();
				if (!r) return;
				if (!ok) { r->state = ResourceState::Failed; return; }
				r->data.cpuData = std::move(*data);
				MGE_PROFILE_SCOPE_N("UploadTexture");
				r->data.gpu = m_renderer.UploadTexture(r->data.cpuData);
				std::error_code ec;
				r->lastModified = std::filesystem::last_write_time(r->path, ec);
				r->state = ResourceState::Ready;
				Logger::Info("Async: texture ready " + r->path);
				});
		}
	}, priority);

	return res;
}

#endif