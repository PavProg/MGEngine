#ifndef SYSTEM_SCHEDULER_HPP
#define SYSTEM_SCHEDULER_HPP

#include "System.hpp"
#include "CommandBuffer.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <thread>
#include <memory>
#include <utility>
#include <vector>

class World;
class JobSystem;

// Хранит системы, раскладывает их по фазам и запускает
class SystemScheduler {
public:
	// Создаем планировщик в main loop
	SystemScheduler(World& world, JobSystem& jobs)
		: m_world(world)
		, m_jobs(jobs)
		, m_mainThread(std::this_thread::get_id()) {

	}

	template<typename T, typename... Args>
	T& AddSystem(Args&&... args) {
		assert(std::this_thread::get_id() == m_mainThread && "AddSystem must run on the main thread");

		assert(!m_running && "Cannot register systems while a phase is running");

		auto system = std::make_unique<T>(std::forward<Args>(args)...);

		T& result = *system;

		const std::size_t phaseIndex = static_cast<std::size_t>(result.GetPhase());

		assert(phaseIndex < m_phases.size());

		// Декларации системы сохраняем один раз при регистрации
		SystemAccess access = result.GetAccess();
		const bool mainThreadOnly = result.RequiresMainThread();

		m_phases[phaseIndex].push_back(SystemEntry{
			std::move(system),
			std::move(access),
			mainThreadOnly
		});

		return result;
	}

	// Выполнить системы фазы и затем применитьт отложенные команды
	void RunPhase(UpdatePhase phase, float dt);

	CommandBuffer& GetCommands() {
		return m_commands;
	}

private:
	struct SystemEntry {
		std::unique_ptr<ISystem> system;
		SystemAccess access;
		bool mainThreadOnly;
	};

	// Выполнить одну систему с разметкой Tracy
	void RunSystem(ISystem& system, float dt);

	// Выполнить совместимую пачку [begin, end) и дождаться
	void ExecuteBatch(
		std::vector<SystemEntry>& systems,
		std::size_t begin,
		std::size_t end,
		float dt
	);

	World& m_world;
	JobSystem& m_jobs;

	std::thread::id m_mainThread;

	bool m_running = false;

	CommandBuffer m_commands;
	std::array<std::vector<SystemEntry>, static_cast<std::size_t>(UpdatePhase::Count)> m_phases;
};

#endif