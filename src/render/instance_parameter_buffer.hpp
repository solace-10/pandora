#pragma once

#include <unordered_map>
#include <vector>
#include <string>

#include "render/material.hpp"

namespace WingsOfSteel
{

class InstanceParameterBuffer
{
public:
    InstanceParameterBuffer() = default;
    ~InstanceParameterBuffer() = default;

    // Pack instance shader parameters into DynamicUniformsData format
    // Returns filled DynamicUniformsData ready for GPU upload
    static DynamicUniformsData PackParameters(
        const std::vector<ShaderParameterDefinition>& definitions,
        const std::vector<std::unordered_map<std::string, float>>& instanceParameters);
};

} // namespace WingsOfSteel
