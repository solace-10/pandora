#pragma once

#include <vector>

#include "core/serialization.hpp"
#include "icomponent.hpp"
#include "component_factory.hpp"
#include "resources/resource.fwd.hpp"
#include "resources/resource_texture_2d.hpp"

namespace WingsOfSteel
{

class LandscapeComponent : public IComponent
{
public:
    LandscapeComponent() = default;
    ~LandscapeComponent() = default;

    uint32_t Seed{ 0 };
    uint32_t Width{ 0 };
    uint32_t Height{ 0 };
    uint32_t Generation{ 0 };
    std::vector<float> Heightmap;
    ResourceTexture2DUniquePtr DebugHeightmapTexture;

    void Deserialize(const ResourceDataStore* pContext, const Json::Data& json) override
    {
        Seed = Json::DeserializeUnsignedInteger(pContext, json, "seed", 0);
    }
};

REGISTER_COMPONENT(LandscapeComponent, "landscape")

} // namespace WingsOfSteel
