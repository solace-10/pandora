#pragma once

#include <glm/vec4.hpp>

#include "component_factory.hpp"
#include "core/serialization.hpp"
#include "icomponent.hpp"

namespace WingsOfSteel
{

class SeaComponent : public IComponent
{
public:
    SeaComponent() = default;
    ~SeaComponent() = default;

    float Width{ 2000.0f }; // Extent along the X axis, in meters, centered on the origin.
    float Length{ 2000.0f }; // Extent along the Z axis, in meters, centered on the origin.
    glm::vec4 Color{ 0.12f, 0.32f, 0.45f, 0.55f };
    uint32_t Generation{ 1 }; // Current generation iteration; used by the renderer to know when the component is stale.

    void Deserialize(const ResourceDataStore* pContext, const Json::Data& json) override
    {
    }
};

REGISTER_COMPONENT(SeaComponent, "sea")

} // namespace WingsOfSteel
