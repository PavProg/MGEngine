#ifndef SYSTEM_SCHEDULER_HPP
#define SYSTEM_SCHEDULER_HPP

#include "System.hpp"
#include "CommandBuffer.hpp"

#include <array>
#include <memory>
#include <utility>
#include <vector>

class World;

// Хранит системы, раскладывает их по фазам и запускает
class SystemScheduler {
public:
	explicit SystemScheduler(World& world) : m_world(world) {}

	template<typename T, typename... Args>
	T& AddSystem(Args&&... args) {
		auto system = std::make_unique<T>(std::forward<Args>(args)...);
		T& ref = *system;
		m_phases[static_cast<std::size_t>(ref.GetPhase())].push_back(std::move(system));
		return ref;
	}

	// Запустить все системы фазы, затем выполнить отложенные команды
	void RunPhase(UpdatePhase phase, float dt);

	CommandBuffer& GetCommands() { return m_commands; }
private:
	World& m_world;
	CommandBuffer m_commands;
	std::array<std::vector<std::unique_ptr<ISystem>>, static_cast<std::size_t>(UpdatePhase::Count)> m_phases;
};

#endif