#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#if defined(TARGET_PLATFORM_WEB)
#include <emscripten/html5.h>
#endif

#include "core/log.hpp"
#include "imgui/imgui.hpp"
#include "input/input_system.hpp"
#include "pandora.hpp"
#include "render/window.hpp"

namespace WingsOfSteel
{

InputCallbackToken InputSystem::m_sToken = 1u;
InputCallbackToken InputSystem::sInvalidInputCallbackToken = 0u;

InputSystem::InputSystem()
{
}

InputSystem::~InputSystem()
{
    GLFWwindow* pWindow = GetWindow()->GetRawWindow();
    glfwSetKeyCallback(pWindow, nullptr);
    glfwSetMouseButtonCallback(pWindow, nullptr);
    glfwSetCursorPosCallback(pWindow, nullptr);
    glfwSetScrollCallback(pWindow, nullptr);
    glfwSetCursorEnterCallback(pWindow, nullptr);
}

void InputSystem::Initialize()
{
    GLFWwindow* pWindow = GetWindow()->GetRawWindow();

    glfwSetKeyCallback(pWindow, [](GLFWwindow* pWindow, int key, int scancode, int action, int mods) {
        GetInputSystem()->HandleKeyboardEvent(key, scancode, action, mods);
    });

    glfwSetMouseButtonCallback(pWindow, [](GLFWwindow* pWindow, int button, int action, int mods) {
        GetInputSystem()->HandleMouseButtonEvent(button, action, mods);
    });

    glfwSetCursorPosCallback(pWindow, [](GLFWwindow* pWindow, double xPos, double yPos) {
        GetInputSystem()->HandleMousePositionEvent(xPos, yPos);
    });

    glfwSetScrollCallback(pWindow, [](GLFWwindow* pWindow, double xOffset, double yOffset) {
        GetInputSystem()->HandleMouseWheelEvent(xOffset, yOffset);
    });

    glfwSetCursorEnterCallback(pWindow, [](GLFWwindow* pWindow, int entered) {
        GetInputSystem()->HandleCursorEnterEvent(entered == GLFW_TRUE);
    });

    // GLFW_CURSOR_DISABLED locks the cursor to the window, hides it and all cursor positions become
    // virtual (unbounded by the window or monitor's resolution).
    // This lets us have an in-game cursor which is consistent between Web and Native, and for the
    // Web client it works in conjunction with `requestPointerLock` to ensure the client gets all
    // mouse button events correctly.
    glfwSetInputMode(pWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // If possible, get the positions based on the raw motion, which is not affected by OS-level
    // settings regarding mouse acceleration.
    if (glfwRawMouseMotionSupported())
    {
        glfwSetInputMode(pWindow, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }
}

void InputSystem::Update()
{
    if (IsCursorLocked())
    {
        ImGui::GetForegroundDrawList()->AddCircleFilled(m_CursorPosition, 4.0f, IM_COL32(255, 0, 0, 255));
    }
}

void InputSystem::HandleMouseWheelEvent(double xOffset, double yOffset)
{
    if (ImGui::GetIO().WantCaptureMouse)
    {
        return;
    }

    const glm::vec2 offset(static_cast<float>(xOffset), static_cast<float>(yOffset));
    for (auto& callbackInfo : m_MouseWheelCallbacks)
    {
        callbackInfo.callback(offset);
    }
}

void InputSystem::HandleKeyboardEvent(int key, int scancode, int action, int mods)
{
    if (ImGui::GetIO().WantCaptureKeyboard)
    {
        return;
    }

    for (auto& callbackInfo : m_KeyboardCallbacks)
    {
        if (callbackInfo.key == key)
        {
            if ((callbackInfo.action == KeyAction::Pressed && action == GLFW_PRESS) || (callbackInfo.action == KeyAction::Released && action == GLFW_RELEASE) || (callbackInfo.action == KeyAction::Held && action == GLFW_REPEAT))
            {
                callbackInfo.callback();
            }
        }
    }
}

void InputSystem::HandleMouseButtonEvent(int button, int action, int mods)
{
    if (ImGui::GetIO().WantCaptureMouse)
    {
        return;
    }

    for (auto& callbackInfo : m_MouseCallbacks)
    {
        if (static_cast<int>(callbackInfo.button) == button)
        {
            if ((callbackInfo.action == MouseAction::Pressed && action == GLFW_PRESS) || (callbackInfo.action == MouseAction::Released && action == GLFW_RELEASE))
            {
                callbackInfo.callback();
            }
        }
    }
}

void InputSystem::HandleMousePositionEvent(double xPos, double yPos)
{
    if (!IsCursorLocked())
    {
        return;
    }

#if defined(TARGET_PLATFORM_WEB)
    // When the cursor is locked, on Web, we receive deltas rather than the expected position.
    // A quirk of GLFW/Emscripten?
    const glm::vec2 cursorDelta(xPos, yPos);
    m_CursorPosition += cursorDelta;
#else
    if (!m_PreviousVirtualCursorPosition)
    {
        m_PreviousVirtualCursorPosition = glm::vec2(xPos, yPos);
    }

    const glm::vec2 currentVirtualCursorPosition(xPos, yPos);
    const glm::vec2 cursorDelta = currentVirtualCursorPosition - m_PreviousVirtualCursorPosition.value();
    m_CursorPosition += cursorDelta;
    m_CursorPosition.x = glm::clamp(m_CursorPosition.x, 0.0f, static_cast<float>(GetWindow()->GetWidth()));
    m_CursorPosition.y = glm::clamp(m_CursorPosition.y, 0.0f, static_cast<float>(GetWindow()->GetHeight()));
    m_PreviousVirtualCursorPosition = currentVirtualCursorPosition;
#endif

    for (auto& callbackInfo : m_MousePositionCallbacks)
    {
        callbackInfo.callback(m_CursorPosition, cursorDelta);
    }
}

void InputSystem::HandleCursorEnterEvent(bool entered)
{
    if (entered)
    {
        m_PreviousVirtualCursorPosition.reset();
    }
}

InputCallbackToken InputSystem::AddKeyboardCallback(InputCallbackKeyboardFn callback, int key, KeyAction action)
{
    InputCallbackKeyboardInfo info;
    info.callback = callback;
    info.key = key;
    info.action = action;
    info.token = GenerateToken();
    m_KeyboardCallbacks.push_back(info);
    return info.token;
}

InputCallbackToken InputSystem::AddMouseButtonCallback(InputCallbackMouseButtonFn callback, MouseButton button, MouseAction action)
{
    InputCallbackMouseInfo info;
    info.callback = callback;
    info.button = button;
    info.action = action;
    info.token = GenerateToken();
    m_MouseCallbacks.push_back(info);
    return info.token;
}

InputCallbackToken InputSystem::AddMouseWheelCallback(InputCallbackMouseWheelFn callback)
{
    InputCallbackMouseWheelInfo info;
    info.callback = callback;
    info.token = GenerateToken();
    m_MouseWheelCallbacks.push_back(info);
    return info.token;
}

InputCallbackToken InputSystem::AddMousePositionCallback(InputCallbackMousePositionFn callback)
{
    InputCallbackMousePositionInfo info;
    info.callback = callback;
    info.token = GenerateToken();
    m_MousePositionCallbacks.push_back(info);
    return info.token;
}

void InputSystem::RemoveKeyboardCallback(InputCallbackToken token)
{
    m_KeyboardCallbacks.remove_if([token](const InputCallbackKeyboardInfo& callbackInfo) { return token == callbackInfo.token; });
}

void InputSystem::RemoveMouseButtonCallback(InputCallbackToken token)
{
    m_MouseCallbacks.remove_if([token](const InputCallbackMouseInfo& callbackInfo) { return token == callbackInfo.token; });
}

void InputSystem::RemoveMouseWheelCallback(InputCallbackToken token)
{
    m_MouseWheelCallbacks.remove_if([token](const InputCallbackMouseWheelInfo& callbackInfo) { return token == callbackInfo.token; });
}

void InputSystem::RemoveMousePositionCallback(InputCallbackToken token)
{
    m_MousePositionCallbacks.remove_if([token](const InputCallbackMousePositionInfo& callbackInfo) { return token == callbackInfo.token; });
}

bool InputSystem::IsCursorLocked() const
{
#if defined(TARGET_PLATFORM_NATIVE)
    return glfwGetWindowAttrib(GetWindow()->GetRawWindow(), GLFW_FOCUSED);
#elif defined(TARGET_PLATFORM_WEB)
    EmscriptenPointerlockChangeEvent pointerLockStatus;
    if (emscripten_get_pointerlock_status(&pointerLockStatus) == EMSCRIPTEN_RESULT_SUCCESS)
    {
        return pointerLockStatus.isActive;
    }
    return false;
#else
    static_assert(false);
#endif
}

} // namespace WingsOfSteel
