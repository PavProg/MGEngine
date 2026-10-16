#ifndef VIEW_HPP
#define VIEW_HPP

#include "ComponentPool.hpp"

#include <algorithm>
#include <tuple>
#include <type_traits>

// Суть. Запросить у миры все сущности у которых есть перечисленные компоненты вместе с ссылками
template<typename... Ts>
class View {
	static_assert(sizeof...(Ts) > 0, "View needs at least one component type");

public:
	explicit View(ComponentPool<std::remove_const_t<Ts>>*... pools)	// принимает пачку типов любой длины
		: m_pools(pools...) {
		m_driver = std::get<0>(m_pools);
		((m_driver = (pools->Size() < m_driver->Size())
			? static_cast<const IComponentPool*>(pools) : m_driver), ...);
	}
	// Верхняя граница количества элементов для дележки
	std::size_t SizeHint() const { return m_driver->Size(); }

	// func(Entity, Ts& ...
	template<typename Func>
	void Each(Func&& func) {
		EachInRange(0, SizeHint(), std::forward<Func>(func));
	}

	// Обработать только позиции [begin, end) - один кусок для одного длжоба
	template<typename Func>
	void EachInRange(std::size_t begin, std::size_t end, Func&& func) {
		const auto& entities = m_driver->Entities();
		end = std::min(end, entities.size());

		for (std::size_t i = begin; i < end; i++) {
			const Entity entity = entities[i];
			if (!(std::get<ComponentPool<std::remove_const_t<Ts>>*>(m_pools)->Has(entity) &&
				...)) {
				continue;
			}
			func(entity, std::get<ComponentPool<std::remove_const_t<Ts>>*>(m_pools)->Get(entity)...);
		}
	}
private:
	std::tuple<ComponentPool<std::remove_const_t<Ts>>*...> m_pools;
	const IComponentPool* m_driver = nullptr;
};

#endif