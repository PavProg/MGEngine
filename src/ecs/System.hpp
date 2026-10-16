#ifndef SYSTEM_HPP
#define SYSTEM_HPP

#include "ComponentType.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

class World;
class CommandBuffer;

// Фазы кадра. Порядок важен: каждая фаза видит результат предыдущих
enum class UpdatePhase {
	Input,	// ввод игрока
	GameLogic,	// ии, квеста боевка
	Animation,	// позы скелетов
	Physics,	// физ движок
	PostPhysics,	// привязки. Меч к руке, камера к голове
	Transform,	// финальные мировые матрицы
	Audio,	// позиция слушателя и источников
	Render,	// отрисовка
	Count
};

// Что система читает и что пишет. По этому описанию планировщик поймет
// какие системы можно запускать параллельно
struct SystemAccess {
	std::vector<ComponentTypeId> reads;
	std::vector<ComponentTypeId> writes;

	template<typename T>
	SystemAccess& Read() { reads.push_back(GetComponentTypeId<T>()); return *this; }

	template<typename T>
	SystemAccess& Write() { writes.push_back(GetComponentTypeId<T>()); return *this; }

	// Конфликт = один пишет то, что другой читает или пишет
	bool ConflictsWith(const SystemAccess& other) const {
		auto contains = [](const std::vector<ComponentTypeId>& v, ComponentTypeId id) {
			return std::find(v.begin(), v.end(), id) != v.end();
		};
		for (ComponentTypeId id : writes) {
			if (contains(other.writes, id) || contains(other.reads, id)) return true;
		}
		for (ComponentTypeId id : other.writes) {
			if (contains(reads, id)) return true;
		}
		return false;
	}
};

class ISystem {
public:
	virtual ~ISystem() = default;

	virtual const char* GetName() const = 0;
	virtual UpdatePhase GetPhase() const = 0;
	virtual SystemAccess GetAccess() const = 0;

	// структурные изменения только через commands
	virtual void Update(World& world, CommandBuffer& commands, float dt) = 0;
};

#endif