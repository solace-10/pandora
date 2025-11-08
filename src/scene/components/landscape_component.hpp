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
    uint32_t Width{ 32 };
    uint32_t Length{ 32 };
    float Height{ 40.0f }; // Maximum height the landscape will be generated to, in meters.
    float CellSize{ 1.0f }; // Cell size, in meters.
    uint32_t Generation{ 0 }; // Current generation iteration; used by the renderer to know when the component is stale.
    uint32_t Octaves{ 4 };
    float Frequency{ 0.002 };
    std::vector<float> Heightmap;
    ResourceTexture2DUniquePtr DebugHeightmapTexture;

    void Deserialize(const ResourceDataStore* pContext, const Json::Data& json) override
    {
        Seed = Json::DeserializeUnsignedInteger(pContext, json, "seed", 0);
    }
};

REGISTER_COMPONENT(LandscapeComponent, "landscape")

} // namespace WingsOfSteel
