#ifndef ANIMATIONSYSTEM_HPP
#define ANIMATIONSYSTEM_HPP

#include "ecs/System.hpp"

// Демо-анимация фигур тестовой сцены (по имени из Tag)
class AnimationSystem final : public ISystem {
    public:
        const char* GetName() const override { return "AnimationSystem"; }
        UpdatePhase GetPhase() const override { return UpdatePhase::Animation; }
        SystemAccess GetAccess() const override;

        void Update(World& world, CommandBuffer& commands, float dt) override;
    private:
        float m_totalTime = 0.0f;
};

#endif
