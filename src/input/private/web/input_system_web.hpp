#pragma once

#if defined(TARGET_PLATFORM_WEB)

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
};

} // namespace WingsOfSteel::Private

#endif // TARGET_PLATFORM_WEB
