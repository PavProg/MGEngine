#include "HierarchyUtils.hpp"
#include "ecs/components/Hierarchy.hpp"

#include <algorithm>
#include <vector>

namespace HierarchyUtils {
	static Hierarchy& GetOrCreateHierarchy(World& world, Entity entity) {
		if (!world.HasComponent<Hierarchy>(entity)) {
			world.AddComponent<Hierarchy>(entity, Hierarchy{});
		}
		return world.GetComponent<Hierarchy>(entity);
	}

	// true, если candidate - это entity  или кто-то из предков
	static bool IsSelfOrAncestor(World& world, Entity candidate, Entity entity) {
		Entity current = entity;
		while (world.IsAlive(current)) {
			if (current == candidate) return true;
			const Hierarchy* h = world.TryGetComponent<Hierarchy>(current);
			current = h ? h->parent : NULL_ENTITY;
		}
		return false;
	}

	// отсоединяем от родителя
	static void DetachFromParent(World& world, Entity child) {
		Hierarchy* childHierarchy = world.TryGetComponent<Hierarchy>(child);
		if (!childHierarchy || childHierarchy->parent.IsNull()) return;

		if (Hierarchy* parentHierarchy = world.TryGetComponent<Hierarchy>(childHierarchy->parent)) {
			auto& kids = parentHierarchy->children;
			kids.erase(std::remove(kids.begin(), kids.end(), child), kids.end());
		}
		childHierarchy->parent = NULL_ENTITY;
	}

	bool SetParent(World& world, Entity child, Entity parent) {
		if (!world.IsAlive(child)) return false;
		if (!parent.IsNull()) {
			if (!world.IsAlive(parent) || IsSelfOrAncestor(world, child, parent)) return false;
		}
		
		DetachFromParent(world, child);
		if (parent.IsNull()) return true;

		// Сначала создаем Hierarchy у обоих, потом берем ссылки
		// (на случай, если добавление компонента сдвинет данные в памяти)
		GetOrCreateHierarchy(world, parent);
		GetOrCreateHierarchy(world, child);

		world.GetComponent<Hierarchy>(child).parent = parent;
		world.GetComponent<Hierarchy>(parent).children.push_back(child);
		return true;
	}

	void DestroyWithChildren(World& world, Entity entity) {
		if (!world.IsAlive(entity)) return;
		DetachFromParent(world, entity);

		// собираем все дерево, потом удаляем (без рекурсии)
		std::vector<Entity> toDestroy{ entity };
		for (std::size_t i = 0; i < toDestroy.size(); ++i) {
			if (const Hierarchy* h = world.TryGetComponent<Hierarchy>(toDestroy[i])) {
				toDestroy.insert(toDestroy.end(), h->children.begin(), h->children.end());
			}
		}
		for (Entity e : toDestroy) {
			world.DestroyEntity(e);
		}
	}
}