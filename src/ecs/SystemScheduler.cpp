#include "SystemScheduler.hpp"
#include "World.hpp"
#include "core/profiler.hpp"

#include <cstring>

void SystemScheduler::RunPhase(UpdatePhase phase, float dt) {
	auto& systems = m_phases[static_cast<std::size_t>(phase)];

	m_world.LockStructure();
	for (auto& system : systems) {
		MGE_PROFILE_SCOPE_N("System");
		MGE_PROFILE_TEXT(system->GetName(), std::strlen(system->GetName()));
		system->Update(m_world, m_commands, dt);
	}
	m_world.UnlockStructure();

	// Структурные изменения строго между фазами
	m_commands.Flush(m_world);
}