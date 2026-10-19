// Bound at group 0, binding 0 for every scene pass. Mirrors RenderSystem::GlobalUniforms.
struct GlobalUniforms
{
    projectionMatrix: mat4x4<f32>,
    viewMatrix: mat4x4<f32>,
    cameraPosition: vec4f,
    time: f32,
    windowWidth: f32,
    windowHeight: f32,
    directionalLightColor: vec4f,
    directionalLightDirection: vec4f,
    ambientLightColor: vec4f,
    ambientSH: array<vec4f, 9> // See AmbientSH in lighting_system.hpp.
};

@group(0) @binding(0) var<uniform> uGlobalUniforms: GlobalUniforms;
