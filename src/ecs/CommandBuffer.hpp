#ifndef COMMAND_BUFFER_HPP
#define COMMAND_BUFFER_HPP

#include "World.hpp"

#include <functional>
#include <mutex>
#include <vector>

// Список отложенных дел для структурных изменений.
// Системы записывают сюда команды, а выполняются они
// разом в главном потоке, когда фаза закончилась.
class CommandBuffer {
public:
	void CreateEntity(std::function<void(World&, Entity)> setup);

	void DestroyEntity(Entity entity);

	template<typename T>
	void AddComponent(Entity entity, T component) {
		Push([entity, component = std::move(component)](World& world) mutable {
			if (world.IsAlive(entity)) {
				world.AddComponent<T>(entity, std::move(component));
			}
		});
	}

	template<typename T>
	void RemoveComponent(Entity entity) {
		Push([entity](World& world) {
			if (world.IsAlive(entity)) {
				world.RemoveComponent<T>(entity);
			}
		});
	}

	// Выполнить все команды по порядку записи. Только из главного потока
	void Flush(World& world);

	bool IsEmpty() const;
private:
	using Command = std::function<void(World&)>;

	void Push(Command command);

	mutable std::mutex m_mutex;
	std::vector<Command> m_commands;
};

#endif