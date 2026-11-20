#ifndef ENTITY_HPP
#define ENTITY_HPP

#include <cstdint>
#include <functional>

// Handle. Номер ячейки + поколение. Когда сущность удаляют поколение растет -> остальные устаревают
struct Entity {
	std::uint32_t index = 0;
	std::uint32_t generation = 0;

	bool IsNull() const { return generation == 0; }	// Аналог nullptr для указателей

	// Перегружаем оператор. Суть сравнить 2 поколения
	bool operator==(const Entity& other) const {
		return index == other.index && generation == other.generation;
	}
	bool operator!=(const Entity& other) const { return !(*this == other); }
};

constexpr Entity NULL_ENTITY{};

namespace std {
	template <>
	struct hash<Entity> {
		size_t operator()(const Entity& e) const noexcept {
			return std::hash<std::uint64_t>{}((std::uint64_t(e.generation) << 32) | e.index);
		}
	};
}

#endif