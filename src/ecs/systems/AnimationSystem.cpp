#include "AnimationSystem.hpp"
#include "ecs/World.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/components/Tag.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>

SystemAccess AnimationSystem::GetAccess() const {
    SystemAccess access;
    access.Read<Tag>().Write<Transform>();
    return access;
}

void AnimationSystem::Update(World& world, CommandBuffer&, float deltaTime) {
    m_totalTime += deltaTime;

    world.GetView<const Tag, Transform>().Each(
        [&](Entity, const Tag& tag, Transform& transform) {
            // Анимация сущностей (красный, синий и желтая фигуры)
            if (tag.name == "RedTriangle") {
                float angle = m_totalTime * 1.0f;
                transform.rotation = glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f));
            } else if (tag.name == "BlueTriangle") {
                float centerX = 2.0f;
                float radius = 0.5f;
                float speed = 2.0f;

                transform.position.x = centerX + radius * std::cos(m_totalTime * speed);
                transform.position.y = 0.0f + radius * std::sin(m_totalTime * speed);
            } else if (tag.name == "YellowSquare") {
                float pulse = 0.8f + 0.2f * std::sin(m_totalTime * 3.0f);
                transform.scale = glm::vec3(pulse, pulse, 1.0f);
            } else if (tag.name == "GreenSquare"){
                float angle = m_totalTime * 0.5f;
                transform.rotation = glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f));
            }   // GreenSquare - точка отсчета в центре сцены
        });
}
