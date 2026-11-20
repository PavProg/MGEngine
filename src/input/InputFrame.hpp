#ifndef INPUT_FRAME_HPP
#define INPUT_FRAME_HPP

#include <bitset>
#include <cstddef>
#include <glm/vec2.hpp>

enum class InputKey : std::size_t {
    W, A, S, D, Q, E, LeftShift, RightShift, Escape, Count
};

struct InputFrame {
    using Keys = std::bitset<static_cast<std::size_t>(InputKey::Count)>;

    Keys down;
    Keys pressed;
    Keys released;
    glm::dvec2 mouseDelta{0.0};
    bool focused = false;
    bool cursorCaptured = false;

    bool IsDown(InputKey key) const {
        return down.test(static_cast<std::size_t>(key));
    }
};

#endif
