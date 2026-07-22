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
    float WaterLevel{ 10.0f };
    float CellSize{ 10.0f }; // Cell size, in meters. Shared by the visual mesh and the physics heightfield.
    uint32_t Generation{ 0 }; // Current generation iteration; used by the renderer to know when the component is stale.
    uint32_t Octaves{ 4 };
    float Frequency{ 0.002 };

    // Island shaping. The falloff mask is subtracted from the terrain height, rather than
    // scaling it, so relief is preserved and the coastline falls wherever the noise happens
    // to cross the water level.
    float FalloffStart{ 0.35f }; // Normalized distance from the center at which land starts being pushed down.
    float FalloffEnd{ 0.95f }; // Normalized distance at which the falloff reaches full strength.
    float FalloffStrength{ 1.0f }; // How much height is subtracted at full falloff, in normalized height.

    // Domain warping distorts the terrain before the falloff is applied, turning what would
    // otherwise be a fairly circular island into one with headlands and inlets.
    float DomainWarpAmp{ 30.0f }; // Warp distance, in cells. Set to 0 to disable warping.
    float DomainWarpFrequency{ 0.01f };

    std::vector<float> Heightmap;
    ResourceTexture2DUniquePtr DebugHeightmapTexture;

    void Deserialize(const ResourceDataStore* pContext, const Json::Data& json) override
    {
        Seed = Json::DeserializeUnsignedInteger(pContext, json, "seed", 0);
    }
};

REGISTER_COMPONENT(LandscapeComponent, "landscape")

} // namespace WingsOfSteel
