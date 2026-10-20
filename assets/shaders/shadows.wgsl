// The directional light's shadow map, rendered by ShadowPass. Include globals.wgsl first.

@group(0) @binding(1) var shadowMap: texture_depth_2d;
@group(0) @binding(2) var shadowSampler: sampler_comparison;

// A world position in the shadow map: xy is its uv, z its depth from the light. Orthographic, so w is 1.
fn shadowMapPosition(worldPosition: vec3f) -> vec3f
{
    let clip = uGlobalUniforms.shadowViewProjection * vec4f(worldPosition, 1.0);
    return vec3f(clip.x * 0.5 + 0.5, 0.5 - clip.y * 0.5, clip.z);
}

fn isOutsideShadowMap(position: vec3f) -> bool
{
    return any(position.xy < vec2f(0.0)) || any(position.xy > vec2f(1.0)) || position.z > 1.0;
}

// How much of the directional light reaches a point: 0 in full shadow, 1 fully lit. Anything
// outside the shadow map is lit. The normal pushes the lookup off the surface, so that the surface
// doesn't shadow itself; use the geometric normal rather than a normal-mapped one.
fn sampleShadow(worldPosition: vec3f, worldNormal: vec3f) -> f32
{
    let position = shadowMapPosition(worldPosition + worldNormal * uGlobalUniforms.shadowParameters.x);
    if (isOutsideShadowMap(position))
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
            lit += textureSampleCompareLevel(shadowMap, shadowSampler, position.xy + vec2f(f32(x), f32(y)) * texelSize, position.z);
        }
    }
    return lit / 9.0;
}

// As sampleShadow, for a point that isn't on a surface, such as one inside a participating medium:
// there is no surface to push off, and a single 2x2 PCF tap is enough for something sampled this often.
fn sampleShadowAt(worldPosition: vec3f) -> f32
{
    let position = shadowMapPosition(worldPosition);
    if (isOutsideShadowMap(position))
    {
        return 1.0;
    }
    return textureSampleCompareLevel(shadowMap, shadowSampler, position.xy, position.z);
}
