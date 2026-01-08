#if defined(TARGET_PLATFORM_WEB)

#include "input/private/web/input_system_web.hpp"

#include <GLFW/glfw3.h>
#include <emscripten/html5.h>

#include "input/input_system.hpp"
#include "pandora.hpp"
#include "render/window.hpp"

namespace WingsOfSteel::Private
{

const std::vector<int>& InputSystemWeb::GetPolledKeys()
{
    static const std::vector<int> keys = {
        // Alphabetic keys (A-Z)
        GLFW_KEY_A, GLFW_KEY_B, GLFW_KEY_C, GLFW_KEY_D, GLFW_KEY_E,
        GLFW_KEY_F, GLFW_KEY_G, GLFW_KEY_H, GLFW_KEY_I, GLFW_KEY_J,
        GLFW_KEY_K, GLFW_KEY_L, GLFW_KEY_M, GLFW_KEY_N, GLFW_KEY_O,
        GLFW_KEY_P, GLFW_KEY_Q, GLFW_KEY_R, GLFW_KEY_S, GLFW_KEY_T,
        GLFW_KEY_U, GLFW_KEY_V, GLFW_KEY_W, GLFW_KEY_X, GLFW_KEY_Y,
        GLFW_KEY_Z,

        // Numeric keys (0-9)
        GLFW_KEY_0, GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3, GLFW_KEY_4,
        GLFW_KEY_5, GLFW_KEY_6, GLFW_KEY_7, GLFW_KEY_8, GLFW_KEY_9,

        // Function keys (F1-F12)
        GLFW_KEY_F1, GLFW_KEY_F2, GLFW_KEY_F3, GLFW_KEY_F4,
        GLFW_KEY_F5, GLFW_KEY_F6, GLFW_KEY_F7, GLFW_KEY_F8,
        GLFW_KEY_F9, GLFW_KEY_F10, GLFW_KEY_F11, GLFW_KEY_F12,

        // Arrow keys
        GLFW_KEY_UP, GLFW_KEY_DOWN, GLFW_KEY_LEFT, GLFW_KEY_RIGHT,

        // Modifier keys
        GLFW_KEY_LEFT_SHIFT, GLFW_KEY_RIGHT_SHIFT,
        GLFW_KEY_LEFT_CONTROL, GLFW_KEY_RIGHT_CONTROL,
        GLFW_KEY_LEFT_ALT, GLFW_KEY_RIGHT_ALT,
        GLFW_KEY_LEFT_SUPER, GLFW_KEY_RIGHT_SUPER,

        // Common control keys
        GLFW_KEY_SPACE, GLFW_KEY_ENTER, GLFW_KEY_ESCAPE,
        GLFW_KEY_TAB, GLFW_KEY_BACKSPACE, GLFW_KEY_DELETE,
        GLFW_KEY_INSERT, GLFW_KEY_HOME, GLFW_KEY_END,
        GLFW_KEY_PAGE_UP, GLFW_KEY_PAGE_DOWN,

        // Punctuation and symbols
        GLFW_KEY_MINUS, GLFW_KEY_EQUAL,
        GLFW_KEY_LEFT_BRACKET, GLFW_KEY_RIGHT_BRACKET,
        GLFW_KEY_BACKSLASH, GLFW_KEY_SEMICOLON,
        GLFW_KEY_APOSTROPHE, GLFW_KEY_GRAVE_ACCENT,
        GLFW_KEY_COMMA, GLFW_KEY_PERIOD, GLFW_KEY_SLASH,

        // Numpad keys
        GLFW_KEY_KP_0, GLFW_KEY_KP_1, GLFW_KEY_KP_2, GLFW_KEY_KP_3,
        GLFW_KEY_KP_4, GLFW_KEY_KP_5, GLFW_KEY_KP_6, GLFW_KEY_KP_7,
        GLFW_KEY_KP_8, GLFW_KEY_KP_9,
        GLFW_KEY_KP_DECIMAL, GLFW_KEY_KP_DIVIDE, GLFW_KEY_KP_MULTIPLY,
        GLFW_KEY_KP_SUBTRACT, GLFW_KEY_KP_ADD, GLFW_KEY_KP_ENTER,

        // Lock and misc keys
        GLFW_KEY_CAPS_LOCK, GLFW_KEY_NUM_LOCK, GLFW_KEY_SCROLL_LOCK,
        GLFW_KEY_PRINT_SCREEN, GLFW_KEY_PAUSE
    };
    return keys;
}

InputSystemWeb::InputSystemWeb(InputSystem& parent)
    : InputSystemImpl(parent)
{
}

InputSystemWeb::~InputSystemWeb()
{
    GLFWwindow* pWindow = GetWindow()->GetRawWindow();
    glfwSetCursorPosCallback(pWindow, nullptr);
    glfwSetScrollCallback(pWindow, nullptr);
    glfwSetCursorEnterCallback(pWindow, nullptr);
}

void InputSystemWeb::Initialize()
{
    GLFWwindow* pWindow = GetWindow()->GetRawWindow();

    glfwSetCursorPosCallback(pWindow, [](GLFWwindow* pWindow, double xPos, double yPos) {
        GetInputSystem()->HandleMousePositionEvent(glm::vec2(xPos, yPos));
    });

    glfwSetScrollCallback(pWindow, [](GLFWwindow* pWindow, double xOffset, double yOffset) {
        GetInputSystem()->HandleMouseWheelEvent(xOffset, yOffset);
    });

    glfwSetCursorEnterCallback(pWindow, [](GLFWwindow* pWindow, int entered) {
        GetInputSystem()->HandleCursorEnterEvent(entered == GLFW_TRUE);
    });

    m_PreviousKeyState.fill(false);
    m_PreviousMouseButtonState.fill(false);
}

void InputSystemWeb::Update()
{
    GLFWwindow* pWindow = GetWindow()->GetRawWindow();

    // Poll keyboard
    for (int key : GetPolledKeys())
    {
        if (key < 0 || key >= static_cast<int>(m_PreviousKeyState.size()))
        {
            continue;
        }

        const bool isPressed = (glfwGetKey(pWindow, key) == GLFW_PRESS);
        const bool wasPressed = m_PreviousKeyState[key];

        if (isPressed && !wasPressed)
        {
            GetInputSystem()->HandleKeyboardEvent(key, 0, GLFW_PRESS, 0);
        }
        else if (!isPressed && wasPressed)
        {
            GetInputSystem()->HandleKeyboardEvent(key, 0, GLFW_RELEASE, 0);
        }

        m_PreviousKeyState[key] = isPressed;
    }

    // Poll mouse buttons (left=0, right=1, middle=2)
    for (int button = 0; button < 3; ++button)
    {
        const bool isPressed = (glfwGetMouseButton(pWindow, button) == GLFW_PRESS);
        const bool wasPressed = m_PreviousMouseButtonState[button];

        if (isPressed && !wasPressed)
        {
            GetInputSystem()->HandleMouseButtonEvent(button, GLFW_PRESS, 0);
        }
        else if (!isPressed && wasPressed)
        {
            GetInputSystem()->HandleMouseButtonEvent(button, GLFW_RELEASE, 0);
        }

        m_PreviousMouseButtonState[button] = isPressed;
    }
}

bool InputSystemWeb::IsCursorLocked() const
{
    EmscriptenPointerlockChangeEvent pointerLockStatus;
    if (emscripten_get_pointerlock_status(&pointerLockStatus) == EMSCRIPTEN_RESULT_SUCCESS)
    {
        return pointerLockStatus.isActive;
    }
    return false;
}

} // namespace WingsOfSteel::Private

#endif // TARGET_PLATFORM_WEB
