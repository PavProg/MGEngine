#ifndef WORLD_HPP
#define WORLD_HPP

#include "Entity.hpp"
#include "ComponentType.hpp"
#include "ComponentPool.hpp"
#include "View.hpp"

#include <atomic>
#include <memory>
#include <vector>

// Правила работы
//	1. Все типы компонентов регистрируются при старте (RegisterComponent).
//	2. "Структурные" изменения - создать/удалить сущность, добавить/убрать
//     компонент - только из главного потока и только когда структура не
//     заблокирована. Внутри систем для этого есть CommandBuffer.
//  3. Пока структура заблокирована, можно сколько угодно читать и менять
//     сами данные компонентов (в том числе из разных потоков).

class World {
public:
	World() = default;
	World(const World&) = delete;
	World& operator=(const World&) = delete;

	// Сущности
	Entity CreateEntity();
	void DestroyEntity(Entity entity);
	bool IsAlive(Entity entity) const;
	std::size_t GetAliveCount() const { return m_aliveCount; }

	// Компоненты
	template<typename T>
	void RegisterComponent();

	template<typename T>
	T& AddComponent(Entity entity, T component);

	template<typename T>
	void RemoveComponent(Entity entity);

	template<typename T>
	bool HasComponent(Entity entity) const; // только читаем

	template<typename T>
	T& GetComponent(Entity entity);

	template<typename T>
	const T& GetComponent(Entity entity) const;

	//nullptr если компонента нет
	template<typename T>
	T* TryGetComponent(Entity entity);

	template<typename T>
	ComponentPool<T>& GetPool();

	template<typename T>
	const ComponentPool<T>& GetPool() const;

	//world.GetView<const Transform, WorldTransform().Each(...)
	template<typename... Ts>
	View<Ts...> GetView();

	void LockStructure() { m_structureLocked = true; }
	void UnlockStructure() { m_structureLocked = false; }
	bool IsStructureLocked() const { return m_structureLocked; }


private:
	std::vector<std::uint32_t> m_generations; // поколение каждой ячейки
	std::vector<std::uint32_t> m_freeIndices; // освободившиеся ячейки
	std::size_t m_aliveCount = 0;

	std::vector<std::unique_ptr<IComponentPool>> m_pools; // индекс = ComponentPoolId

	std::atomic<bool> m_structureLocked{ false };

	template<typename T>
	bool IsRegistered() const;
};

#include "World.tpp"

#endif