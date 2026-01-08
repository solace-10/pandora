#pragma once

#if defined(TARGET_PLATFORM_WEB)

#include <array>
#include <vector>

#include "input/private/input_system_impl.hpp"

namespace WingsOfSteel::Private
{

class InputSystemWeb : public InputSystemImpl
{
public:
    InputSystemWeb(InputSystem& parent);
    ~InputSystemWeb() override;

    void Initialize() override;
    void Update() override;
    bool IsCursorLocked() const override;

private:
    static const std::vector<int>& GetPolledKeys();

    std::array<bool, 512> m_PreviousKeyState{};
    std::array<bool, 3> m_PreviousMouseButtonState{};
};

} // namespace WingsOfSteel::Private

#endif // TARGET_PLATFORM_WEB
