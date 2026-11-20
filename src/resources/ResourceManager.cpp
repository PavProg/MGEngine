#include "ResourceManager.hpp"
#include "render/IRenderAdapter.hpp"
#include "logger/logger.hpp"

ResourceManager::ResourceManager(IRenderAdapter& renderer, JobSystem& jobs) 
	: m_renderer(renderer) {
	m_queue.SetJobSystem(&jobs);
}

ResourceManager::~ResourceManager() {
	ReleasePlaceholders();
	Clear();
}

void ResourceManager::Clear() {
	m_caches.clear();
	Logger::Info("ResourceManager: cleared");
}

void ResourceManager::InitPlaceholders() {
	// белая текстура 1х1
	TextureData white;
	white.width = 1;
	white.height = 1;
	white.channels = 4;
	white.pixels = { 255, 255, 255, 255 };
	white.filter = TextureFilter::Nearest;
	white.wrap = TextureWrap::Repeat;
	m_placeholderTex = m_renderer.UploadTexture(white);

	MeshData quad;
	quad.vertices = {
		{{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
		{{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
		{{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
		{{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
	};
	quad.indices = { 0, 1, 2, 0, 2, 3 };
	m_placeholderMesh = m_renderer.UploadMesh(quad);

	Logger::Info("ResourceManager: placeholders created");
}

void ResourceManager::ReleasePlaceholders() {
	if (m_placeholderTex.textureId) m_renderer.ReleaseTexture(m_placeholderTex);
	if (m_placeholderMesh.vao) m_renderer.ReleaseMesh(m_placeholderMesh);
}