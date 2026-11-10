#ifndef INPUT_SERVICE_HPP
#define INPUT_SERVICE_HPP

#include "InputFrame.hpp"

struct GLFWwindow;

// Owns the window's input callbacks and user pointer. Destroy before the window.
class InputService {
public:
    explicit InputService(GLFWwindow& window);
    ~InputService();
    InputService(const InputService&) = delete;
    InputService& operator=(const InputService&) = delete;

    // Main thread, once per render frame, before running the Input phase.
    void PollEvents();
    const InputFrame& GetFrame() const { return m_frame; }
    void SetGameplayInputEnabled(bool enabled);

private:
    static InputService& FromWindow(GLFWwindow* window);
    static void OnKey(GLFWwindow* window, int key, int, int action, int);
    static void OnMouseButton(GLFWwindow* window, int button, int action, int);
    static void OnCursorPosition(GLFWwindow* window, double x, double y);
    static void OnFocus(GLFWwindow* window, int focused);

    void SetCursorCaptured(bool captured);

    GLFWwindow* m_window;
    InputFrame m_frame;
    glm::dvec2 m_previousCursor{0.0};
    bool m_hasPreviousCursor = false;
    bool m_gameplayInputEnabled = false;
};

#endif
