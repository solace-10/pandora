// The filters from Jimenez, "Next Generation Post Processing in Call of Duty: Advanced Warfare"
// (SIGGRAPH 2014).
#include "/shaders/pandora/fullscreen_triangle.wgsl"

@group(0) @binding(0) var source: texture_2d<f32>;
@group(0) @binding(1) var sourceSampler: sampler;

fn tap(uv: vec2f, x: f32, y: f32) -> vec3f
{
    let texelSize = 1.0 / vec2f(textureDimensions(source));
    return textureSampleLevel(source, sourceSampler, uv + vec2f(x, y) * texelSize, 0.0).rgb;
}

@fragment fn downsampleMain(in: VertexOutput) -> @location(0) vec4f
{
    let a = tap(in.uv, -2.0, -2.0);
    let b = tap(in.uv, 0.0, -2.0);
    let c = tap(in.uv, 2.0, -2.0);
    let d = tap(in.uv, -2.0, 0.0);
    let e = tap(in.uv, 0.0, 0.0);
    let f = tap(in.uv, 2.0, 0.0);
    let g = tap(in.uv, -2.0, 2.0);
    let h = tap(in.uv, 0.0, 2.0);
    let i = tap(in.uv, 2.0, 2.0);
    let j = tap(in.uv, -1.0, -1.0);
    let k = tap(in.uv, 1.0, -1.0);
    let l = tap(in.uv, -1.0, 1.0);
    let m = tap(in.uv, 1.0, 1.0);

    // Five overlapping 2x2 boxes: the centre one carries half the weight, the four corner ones the rest.
    let color = e * 0.125 + (a + c + g + i) * 0.03125 + (b + d + f + h) * 0.0625 + (j + k + l + m) * 0.125;
    return vec4f(color, 1.0);
}

// A 3x3 tent over the smaller level. The blend state adds it onto the larger one.
@fragment fn upsampleMain(in: VertexOutput) -> @location(0) vec4f
{
    let corners = tap(in.uv, -1.0, -1.0) + tap(in.uv, 1.0, -1.0) + tap(in.uv, -1.0, 1.0) + tap(in.uv, 1.0, 1.0);
    let edges = tap(in.uv, 0.0, -1.0) + tap(in.uv, -1.0, 0.0) + tap(in.uv, 1.0, 0.0) + tap(in.uv, 0.0, 1.0);
    return vec4f((corners + edges * 2.0 + tap(in.uv, 0.0, 0.0) * 4.0) / 16.0, 1.0);
}
