#pragma once

#include "core/serialization.hpp"
#include "icomponent.hpp"
#include "component_factory.hpp"

namespace WingsOfSteel
{

class LandscapeComponent : public IComponent
{
public:
    LandscapeComponent() = default;
    ~LandscapeComponent() = default;

    uint32_t m_Seed{ 0 };

    void Deserialize(const ResourceDataStore* pContext, const Json::Data& json) override
    {
        m_Seed = Json::DeserializeUnsignedInteger(pContext, json, "seed", 0);
    }
};

REGISTER_COMPONENT(LandscapeComponent, "landscape")

} // namespace WingsOfSteel
