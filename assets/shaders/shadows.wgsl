// The directional light's shadow map, rendered by ShadowPass. Include globals.wgsl first.

@group(0) @binding(1) var shadowMap: texture_depth_2d;
@group(0) @binding(2) var shadowSampler: sampler_comparison;

// How much of the directional light reaches a point: 0 in full shadow, 1 fully lit. Anything
// outside the shadow map is lit. The normal pushes the lookup off the surface, so that the surface
// doesn't shadow itself; use the geometric normal rather than a normal-mapped one.
fn sampleShadow(worldPosition: vec3f, worldNormal: vec3f) -> f32
{
    let position = worldPosition + worldNormal * uGlobalUniforms.shadowParameters.x;
    let clip = uGlobalUniforms.shadowViewProjection * vec4f(position, 1.0); // Orthographic, so w is 1.
    let uv = vec2f(clip.x * 0.5 + 0.5, 0.5 - clip.y * 0.5);
    if (any(uv < vec2f(0.0)) || any(uv > vec2f(1.0)) || clip.z > 1.0)
    {
        return 1.0;
    }

    // 3x3 taps of the comparison sampler's bilinear 2x2 PCF.
    let texelSize = 1.0 / vec2f(textureDimensions(shadowMap));
    var lit = 0.0;
    for (var y = -1; y <= 1; y++)
    {
        for (var x = -1; x <= 1; x++)
        {
            lit += textureSampleCompareLevel(shadowMap, shadowSampler, uv + vec2f(f32(x), f32(y)) * texelSize, clip.z);
        }
    }
    return lit / 9.0;
}
