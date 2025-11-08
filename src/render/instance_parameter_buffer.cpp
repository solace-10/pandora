#include "render/instance_parameter_buffer.hpp"

namespace WingsOfSteel
{

DynamicUniformsData InstanceParameterBuffer::PackParameters(
    const std::vector<ShaderParameterDefinition>& definitions,
    const std::vector<std::unordered_map<std::string, float>>& instanceParameters)
{
    DynamicUniformsData data;

    // Initialize all parameters to default values first
    for (const auto& def : definitions)
    {
        const size_t paramIndex = def.offset / 4; // Each vec4 can hold 4 floats
        const size_t componentOffset = def.offset % 4;

        if (paramIndex >= DynamicUniformsData::MaxParams)
        {
            continue; // Skip if we exceed max params
        }

        // Set default values
        for (uint32_t i = 0; i < def.componentCount; i++)
        {
            const size_t finalParamIndex = (def.offset + i) / 4;
            const size_t finalComponentOffset = (def.offset + i) % 4;

            if (finalParamIndex < DynamicUniformsData::MaxParams)
            {
                data.params[finalParamIndex][finalComponentOffset] = def.defaultValue[i];
            }
        }
    }

    // Override with instance-specific values
    if (!instanceParameters.empty())
    {
        for (const auto& params : instanceParameters)
        {
            for (const auto& def : definitions)
            {
                auto it = params.find(def.name);
                if (it != params.end())
                {
                    const size_t paramIndex = def.offset / 4;
                    const size_t componentOffset = def.offset % 4;

                    if (paramIndex < DynamicUniformsData::MaxParams)
                    {
                        // For now, we only support float parameters
                        // Vec2/Vec3/Vec4 would need to be set component by component
                        data.params[paramIndex][componentOffset] = it->second;
                    }
                }
            }
        }
    }

    return data;
}

} // namespace WingsOfSteel
