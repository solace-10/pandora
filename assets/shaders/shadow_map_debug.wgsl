// Draws the shadow map's depth as greyscale for the Lighting debug window (see
// ShadowPass::RequestDebugView). White is the far plane, where nothing was drawn.
#include "/shaders/pandora/fullscreen_triangle.wgsl"

@group(0) @binding(0) var shadowMap: texture_depth_2d;

@fragment fn fragmentMain(in: VertexOutput) -> @location(0) vec4f
{
    let size = vec2f(textureDimensions(shadowMap));
    let depth = textureLoad(shadowMap, vec2i(in.uv * size), 0);
    return vec4f(vec3f(depth), 1.0);
}
