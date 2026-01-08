#if defined(TARGET_PLATFORM_WEB)

#include "input/private/web/input_system_web.hpp"

#include <GLFW/glfw3.h>
#include <emscripten/html5.h>

#include "core/log.hpp"
#include "input/input_system.hpp"
#include "pandora.hpp"
#include "render/window.hpp"

namespace WingsOfSteel::Private
{

InputSystemWeb::InputSystemWeb(InputSystem& parent)
    : InputSystemImpl(parent)
{
}

InputSystemWeb::~InputSystemWeb()
{
    GLFWwindow* pWindow = GetWindow()->GetRawWindow();
    glfwSetKeyCallback(pWindow, nullptr);
    glfwSetMouseButtonCallback(pWindow, nullptr);
    glfwSetCursorPosCallback(pWindow, nullptr);
    glfwSetScrollCallback(pWindow, nullptr);
    glfwSetCursorEnterCallback(pWindow, nullptr);
}

void InputSystemWeb::Initialize()
{
    GLFWwindow* pWindow = GetWindow()->GetRawWindow();

    glfwSetKeyCallback(pWindow, [](GLFWwindow* pWindow, int key, int scancode, int action, int mods) {
        GetInputSystem()->HandleKeyboardEvent(key, scancode, action, mods);
    });

    glfwSetMouseButtonCallback(pWindow, [](GLFWwindow* pWindow, int button, int action, int mods) {
        GetInputSystem()->HandleMouseButtonEvent(button, action, mods);
    });

    glfwSetCursorPosCallback(pWindow, [](GLFWwindow* pWindow, double xPos, double yPos) {

                             
        GetInputSystem()->HandleMousePositionEvent(glm::vec2(xPos, yPos));
    });

    glfwSetScrollCallback(pWindow, [](GLFWwindow* pWindow, double xOffset, double yOffset) {
        GetInputSystem()->HandleMouseWheelEvent(xOffset, yOffset);
    });

    glfwSetCursorEnterCallback(pWindow, [](GLFWwindow* pWindow, int entered) {
        GetInputSystem()->HandleCursorEnterEvent(entered == GLFW_TRUE);
    });
}

void InputSystemWeb::Update()
{    
        int result = glfwGetMouseButton(GetWindow()->GetRawWindow(), GLFW_MOUSE_BUTTON_LEFT);
        Log::Info() << "Mouse left: " << result;
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
