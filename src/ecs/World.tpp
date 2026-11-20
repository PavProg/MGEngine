#ifndef WORLD_TPP
#define WORLD_TPP

#include <cassert>

template<typename T>
void World::RegisterComponent() {
    assert(!m_structureLocked && "RegisterComponent while structure is locked");

    const ComponentTypeId id = GetComponentTypeId<T>();
    if (id >= m_pools.size()) {
        m_pools.resize(id + 1);
    }
    if (!m_pools[id]) {
        m_pools[id] = std::make_unique<ComponentPool<T>>();
    }
}

template<typename T>
bool World::IsRegistered() const {
    const ComponentTypeId id = GetComponentTypeId<T>();
    return id < m_pools.size() && m_pools[id] != nullptr;
}

template<typename T>
ComponentPool<T>& World::GetPool() {
    assert(IsRegistered<T>() && "Component type is not registered");
    return *static_cast<ComponentPool<T>*>(m_pools[GetComponentTypeId<T>()].get());
}

template<typename T>
const ComponentPool<T>& World::GetPool() const {
    assert(IsRegistered<T>() && "Component type is not registered");
    return *static_cast<const ComponentPool<T>*>(m_pools[GetComponentTypeId<T>()].get());
}

template<typename T>
T& World::AddComponent(Entity entity, T component) {
    assert(!m_structureLocked && "AddComponent inside a system: use CommandBuffer");
    assert(IsAlive(entity) && "AddComponent to dead entity");
    return GetPool<T>().Add(entity, std::move(component));
}

template<typename T>
void World::RemoveComponent(Entity entity) {
    assert(!m_structureLocked && "RemoveComponent inside a system: use CommandBuffer");
    GetPool<T>().Remove(entity);
}

template<typename T>
bool World::HasComponent(Entity entity) const {
    // Незарегистрированный тип - просто "нет компонента", без падения
    return IsRegistered<T>() && GetPool<T>().Has(entity);
}

template<typename T>
T& World::GetComponent(Entity entity) {
    return GetPool<T>().Get(entity);
}

template<typename T>
const T& World::GetComponent(Entity entity) const {
    return GetPool<T>().Get(entity);
}

template<typename T>
T* World::TryGetComponent(Entity entity) {
    return IsRegistered<T>() ? GetPool<T>().TryGet(entity) : nullptr;
}

template<typename... Ts>
View<Ts...> World::GetView() {
    return View<Ts...>(&GetPool<std::remove_const_t<Ts>>()...);
}

#endif