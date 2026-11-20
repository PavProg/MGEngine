#include "InputService.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <array>
#include <cassert>

namespace {
    // Indexed by InputKey. GLFW codes stay in the device layer.
    constexpr std::array glfwKeys{
        GLFW_KEY_W, GLFW_KEY_A, GLFW_KEY_S, GLFW_KEY_D, GLFW_KEY_Q, GLFW_KEY_E,
        GLFW_KEY_LEFT_SHIFT, GLFW_KEY_RIGHT_SHIFT, GLFW_KEY_ESCAPE
    };
    static_assert(glfwKeys.size() == static_cast<std::size_t>(InputKey::Count));
}

InputService::InputService(GLFWwindow& window) : m_window(&window) {
    assert(!glfwGetWindowUserPointer(m_window));
    m_frame.focused = glfwGetWindowAttrib(m_window, GLFW_FOCUSED) == GLFW_TRUE;
    glfwSetWindowUserPointer(m_window, this);
    glfwSetKeyCallback(m_window, OnKey);
    glfwSetMouseButtonCallback(m_window, OnMouseButton);
    glfwSetCursorPosCallback(m_window, OnCursorPosition);
    glfwSetWindowFocusCallback(m_window, OnFocus);
}

InputService::~InputService() {
    SetCursorCaptured(false);
    glfwSetKeyCallback(m_window, nullptr);
    glfwSetMouseButtonCallback(m_window, nullptr);
    glfwSetCursorPosCallback(m_window, nullptr);
    glfwSetWindowFocusCallback(m_window, nullptr);
    glfwSetWindowUserPointer(m_window, nullptr);
}

void InputService::PollEvents() {
    m_frame.pressed.reset();
    m_frame.released.reset();
    m_frame.mouseDelta = glm::dvec2(0.0);
    glfwPollEvents();
}

void InputService::SetGameplayInputEnabled(bool enabled) {
    m_gameplayInputEnabled = enabled;
    if (!enabled) SetCursorCaptured(false);
}

InputService& InputService::FromWindow(GLFWwindow* window) {
    return *static_cast<InputService*>(glfwGetWindowUserPointer(window));
}

void InputService::OnKey(GLFWwindow* window, int key, int, int action, int) {
    auto& input = FromWindow(window);
    if (!input.m_frame.focused || action == GLFW_REPEAT) return;

    for (std::size_t index = 0; index < glfwKeys.size(); ++index) {
        if (glfwKeys[index] != key) continue;
        if (action == GLFW_PRESS) {
            input.m_frame.down.set(index);
            input.m_frame.pressed.set(index);
            if (key == GLFW_KEY_ESCAPE) input.SetCursorCaptured(false);
        } else if (action == GLFW_RELEASE) {
            input.m_frame.down.reset(index);
            input.m_frame.released.set(index);
        }
        return;
    }
}

void InputService::OnMouseButton(GLFWwindow* window, int button, int action, int) {
    auto& input = FromWindow(window);
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS &&
        input.m_frame.focused && input.m_gameplayInputEnabled) {
        input.SetCursorCaptured(true);
    }
}

void InputService::OnCursorPosition(GLFWwindow* window, double x, double y) {
    auto& input = FromWindow(window);
    if (!input.m_frame.cursorCaptured) return;

    const glm::dvec2 position(x, y);
    if (input.m_hasPreviousCursor) input.m_frame.mouseDelta += position - input.m_previousCursor;
    input.m_previousCursor = position;
    input.m_hasPreviousCursor = true;
}

void InputService::OnFocus(GLFWwindow* window, int focused) {
    auto& input = FromWindow(window);
    input.m_frame.focused = focused == GLFW_TRUE;
    if (!input.m_frame.focused) {
        input.m_frame.released |= input.m_frame.down;
        input.m_frame.down.reset();
        input.m_frame.pressed.reset();
        input.SetCursorCaptured(false);
    }
}

void InputService::SetCursorCaptured(bool captured) {
    if (m_frame.cursorCaptured == captured) return;

    m_frame.cursorCaptured = captured;
    glfwSetInputMode(m_window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, captured ? GLFW_TRUE : GLFW_FALSE);
    }
    m_hasPreviousCursor = false;
    m_frame.mouseDelta = glm::dvec2(0.0);
}
