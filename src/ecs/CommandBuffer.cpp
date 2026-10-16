#include "CommandBuffer.hpp"

void CommandBuffer::CreateEntity(std::function<void(World&, Entity)> setup) {
	Push([setup = std::move(setup)](World& world) {
		Entity entity = world.CreateEntity();
		if (setup) setup(world, entity);
	});
}

void CommandBuffer::DestroyEntity(Entity entity) {
	Push([entity](World& world) {
		world.DestroyEntity(entity);
	});
}

void CommandBuffer::Push(Command command) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_commands.push_back(std::move(command));
}

void CommandBuffer::Flush(World& world) {
	std::vector<Command> commands;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		commands.swap(m_commands);
	}

	for (auto& command : commands) {
		command(world);
	}
}

bool CommandBuffer::IsEmpty() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_commands.empty();
}