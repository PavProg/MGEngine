#include "SystemScheduler.hpp"

#include "World.hpp"
#include "core/profiler.hpp"
#include "core/JobSystem.hpp"


#include <cstring>
#include <cstdint>
#include <exception>
#include <limits>
#include <stdexcept>

void SystemScheduler::RunSystem(ISystem& system, float dt) {
	MGE_PROFILE_SCOPE_N("System");

	const char* name = system.GetName();
	MGE_PROFILE_TEXT(name, std::strlen(name));

	system.Update(m_world, m_commands, dt);
}

void SystemScheduler::ExecuteBatch(
	std::vector<SystemEntry>& systems,
	std::size_t begin,
	std::size_t end,
	float dt
) {
	assert(begin <= end);
	assert(end <= systems.size());

	const size_t count = end - begin;

	if (count == 0) {
		return;
	}

	// Если одна система параллельная задача не нужна
	if (count == 1) {
		RunSystem(*systems[begin].system, dt);
		return;
	}

	if (count > std::numeric_limits<uint32_t>::max()) {
		throw std::length_error("Too many systems in batch");
	}

	MGE_PROFILE_SCOPE_N("SystemBatch");

	std::vector<std::exception_ptr> errors(count);

	m_jobs.ParallelFor(static_cast<std::uint32_t>(count), 1,
		[this, &systems, &errors, begin, dt](
			std::uint32_t rangeBegin,
			std::uint32_t rangeEnd
		) {
			for (std::uint32_t offset = rangeBegin; offset < rangeEnd; ++offset) {
				try {
					RunSystem(*systems[begin + offset].system, dt);

				}
				catch (...) {
					errors[offset] = std::current_exception();
				}
			}
		}
	);

	for (const std::exception_ptr& error : errors) {
		if (error) {
			std::rethrow_exception(error);
		}
	}
}

void SystemScheduler::RunPhase(UpdatePhase phase, float dt) {
	assert(std::this_thread::get_id() == m_mainThread && "RunPhase must run on the main thread");

	assert(!m_running && "RunPhase cannot be called recursively");
	assert(!m_world.IsStructureLocked());

	const std::size_t phaseIndex = static_cast<std::size_t>(phase);

	assert(phaseIndex < m_phases.size());

	auto& systems = m_phases[phaseIndex];

	MGE_PROFILE_SCOPE_N("RunPhase");

	m_running = true;
	m_world.LockStructure();

	try {
		std::size_t begin = 0;

		while (begin < systems.size()) {
			// система из главного потока отдельно
			if (systems[begin].mainThreadOnly) {
				RunSystem(*systems[begin].system, dt);
				++begin;
				continue;
			}

			std::size_t end = begin + 1;

			while (end < systems.size()) {
				const SystemEntry& candidate = systems[end];

				if (candidate.mainThreadOnly) {
					break;
				}

				bool conflict = false;

				for (std::size_t i = begin; i < end; ++i) {
					if (systems[i].access.ConflictsWith(candidate.access)) {
						conflict = true;
						break;
					}
				}

				if (conflict) {
					break;
				}

				++end;
			}

			// Возвращается только после завершения всей пачки
			ExecuteBatch(systems, begin, end, dt);

			begin = end;
		}
		m_world.UnlockStructure();
		m_commands.Flush(m_world);

		m_running = false;
	}
	catch (...) {
		// Если ошибка служебные флаги выключить
		m_world.UnlockStructure();
		m_running = false;

		throw;
	}
}