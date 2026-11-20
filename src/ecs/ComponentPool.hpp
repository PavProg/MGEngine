#ifndef COMPONENT_POOL_HPP
#define COMPONENT_POOL_HPP

#include "Entity.hpp"

#include <cassert>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

// Общий родитель для хранения transform, mesh и тп независимо от типа T. Для удобства обращения из World.
class IComponentPool {
public:
	virtual ~IComponentPool() = default;
	virtual bool Has(Entity entity) const = 0;
	virtual void Remove(Entity entity) = 0;
	virtual std::size_t Size() const = 0;
	virtual const std::vector<Entity>& Entities() const = 0;
};

// Хранилище компонентов одного типа - sparse set
template<typename T>
class ComponentPool final : public IComponentPool {
public:
	// добавить/заменить компонент
	T& Add(Entity entity, T component) {
		if (Has(entity)) {
			T& existing = m_data[m_sparse[entity.index]];
			existing = std::move(component);
			return existing;
		}

		if (entity.index >= m_sparse.size()) {
			m_sparse.resize(entity.index + 1, EMPTY);
		}

		m_sparse[entity.index] = static_cast<std::uint32_t>(m_entities.size());
		m_entities.push_back(entity);
		m_data.push_back(std::move(component));
		return m_data.back();
	}

	void Remove(Entity entity) override {
		if (!Has(entity)) return;

		const std::uint32_t removed = m_sparse[entity.index];
		const std::uint32_t last = static_cast<std::uint32_t>(m_entities.size() - 1);

		if (removed != last) {
			m_data[removed] = std::move(m_data[last]);
			m_entities[removed] = m_entities[last];
			m_sparse[m_entities[removed].index] = removed;
		}

		m_data.pop_back();
		m_entities.pop_back();
		m_sparse[entity.index] = EMPTY;
	}

	// отсекает мертвые handle
	bool Has(Entity entity) const override {
		return entity.index < m_sparse.size()
			&& m_sparse[entity.index] != EMPTY
			&& m_entities[m_sparse[entity.index]] == entity;
	}

	T& Get(Entity entity) {
		assert(Has(entity) && "Component not found");
		return m_data[m_sparse[entity.index]];
	}

	const T& Get(Entity entity) const {
		assert(Has(entity) && "Component not found");
		return m_data[m_sparse[entity.index]];
	}

	T* TryGet(Entity entity) {
		return Has(entity) ? &m_data[m_sparse[entity.index]] : nullptr;
	}

	const T* TryGet(Entity entity) const {
		return Has(entity) ? &m_data[m_sparse[entity.index]] : nullptr;
	}

	std::size_t Size() const override { return m_entities.size(); }
	const std::vector<Entity>& Entities() const override { return m_entities; }

	// для прямого доступа к джобам
	std::vector<T>& Data() { return m_data; }
	const std::vector<T>& Data() const { return m_data; }
private:
	static constexpr std::uint32_t EMPTY = std::numeric_limits<std::uint32_t>::max();

	std::vector<std::uint32_t> m_sparse;
	std::vector<Entity> m_entities;
	std::vector<T> m_data;
};

#endif