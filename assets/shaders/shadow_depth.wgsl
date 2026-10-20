// Renders models into the directional light's shadow map (see ShadowPass). Only the position is
// read, so it works for any model whatever its material's shader.

struct LocalUniforms
{
    modelMatrix: mat4x4<f32>
};

struct InstanceUniforms
{
    transform: array<mat4x4<f32>>
};

#include "/shaders/pandora/globals.wgsl"
@group(1) @binding(0) var<uniform> uLocalUniforms: LocalUniforms;
@group(2) @binding(0) var<storage, read> uInstanceUniforms: InstanceUniforms;

@vertex fn vertexMain(@builtin(instance_index) instanceIdx: u32, @location(0) position: vec3f) -> @builtin(position) vec4f
{
    let modelMatrix = uInstanceUniforms.transform[instanceIdx] * uLocalUniforms.modelMatrix;
    return uGlobalUniforms.shadowViewProjection * modelMatrix * vec4f(position, 1.0);
}
