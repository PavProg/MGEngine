#include "TextureLoader.hpp"
#include "ResourceManager.hpp"
#include "render/IRenderAdapter.hpp"
#include "logger/logger.hpp"
#include "core/profiler.hpp"

#include "stb/stb_image.h"

#include <filesystem>

namespace TextureLoader {
	std::shared_ptr<Resource<Texture>> Load(const std::string& path, IRenderAdapter& renderer) {
		// Phase 1. переиспользуем ParseToData
		TextureData data;
		if (!ParseToData(path, data)) {
			return nullptr;
		}

		// Phase 2. GPU
		Resource<Texture>* raw = new Resource<Texture>{};
		raw->path = path;
		raw->data.cpuData = std::move(data);
		raw->data.gpu = renderer.UploadTexture(raw->data.cpuData);

		std::error_code ec;
		raw->lastModified = std::filesystem::last_write_time(path, ec);

		Logger::Info("TextureLoader: " + path + " ("
			+ std::to_string(raw->data.cpuData.width) + "x"
			+ std::to_string(raw->data.cpuData.height) + ", "
			+ std::to_string(raw->data.cpuData.channels) + " channels)");

		auto deleter = [&renderer](Resource<Texture>* p) {
			renderer.ReleaseTexture(p->data.gpu);
			delete p;
		};

		return std::shared_ptr<Resource<Texture>>(raw, deleter);
	}

	bool ParseToData(const std::string& path, TextureData& out) {
		stbi_set_flip_vertically_on_load(true);

		int w, h, ch;
		unsigned char* pixels = stbi_load(path.c_str(), &w, &h, &ch, 0);
		if (!pixels) {
			Logger::Error(std::string("TextureLoader::ParseToData: stbi_load failed for ")
				+ path + " (" + (stbi_failure_reason() ? stbi_failure_reason() : "?") + ")");
			return false;
		}

		out.width = w;
		out.height = h;
		out.channels = ch;
		out.pixels.assign(pixels, pixels + (w * h * ch));
		out.filter = TextureFilter::Nearest;
		out.wrap = TextureWrap::Repeat;

		stbi_image_free(pixels);

		return true;
	}
}

template<>
std::shared_ptr<Resource<Texture>>
ResourceManager::LoadImpl<Texture>(const std::string& path) {
	return TextureLoader::Load(path, m_renderer);
}

void ResourceManager::ReloadModifiedTextures() {
	MGE_PROFILE_SCOPE_N("HotReloadTextures");
	auto typeIt = m_caches.find(std::type_index(typeid(Texture)));
	if (typeIt == m_caches.end()) return;

	for (auto& [path, weak] : typeIt->second) {
		auto alive = weak.lock();
		if (!alive) continue;
		auto res = std::static_pointer_cast<Resource<Texture>>(alive);

		if (res->state.load() != ResourceState::Ready) continue;	// Грузящиеся ресурсы не трогаем

		std::error_code ec;
		auto current = std::filesystem::last_write_time(res->path, ec);
		if (ec || current <= res->lastModified) continue;

		stbi_set_flip_vertically_on_load(true);
		int w, h, ch;
		unsigned char* pixels = stbi_load(res->path.c_str(), &w, &h, &ch, 0);
		if (!pixels) {
			Logger::Warning("HotReload: cannot read texture " + res->path);
			res->lastModified = current;
			continue;
		}

		TextureData nd;
		nd.width = w; nd.height = h; nd.channels = ch;
		nd.pixels.assign(pixels, pixels + (w * h * ch));
		nd.filter = res->data.cpuData.filter;
		nd.wrap = res->data.cpuData.wrap;
		stbi_image_free(pixels);

		GPUTexture newGpu = m_renderer.UploadTexture(nd);

		m_renderer.ReleaseTexture(res->data.gpu);
		res->data.cpuData = std::move(nd);
		res->data.gpu = newGpu;
		res->lastModified = current;

		Logger::Info("HotReload: reloaded texture " + res->path);
	}
}