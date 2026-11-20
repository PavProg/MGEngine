#ifndef COMPONENT_TYPE_HPP
#define COMPONENT_TYPE_HPP

#include <atomic>
#include <cstdint>
#include <type_traits>

// P.S. Порядковый номер, не сохраняем в файлы и не передаем по сети!!!
using ComponentTypeId = std::uint32_t;

namespace detail {
	inline std::atomic<ComponentTypeId> g_nextComponentTypeId{ 0 };

	template<typename T>
	ComponentTypeId ComponentTypeIdImpl() {
		static const ComponentTypeId id = g_nextComponentTypeId++;
		return id;
	}
}

template<typename T>
ComponentTypeId GetComponentTypeId() {
	return detail::ComponentTypeIdImpl<std::remove_cv_t<T>>();
}

#endif