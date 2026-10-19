#include "/shaders/pandora/fullscreen_triangle.wgsl"

struct Settings
{
    exposure: f32,
    bloomStrength: f32,
    // The upsample adds every level together; this turns the sum back into an average.
    bloomScale: f32,
    // A Tonemapper.
    tonemapper: u32
};

@group(0) @binding(0) var sceneTexture: texture_2d<f32>;
@group(0) @binding(1) var<uniform> settings: Settings;
@group(0) @binding(2) var overlayTexture: texture_2d<f32>;
@group(0) @binding(3) var bloomTexture: texture_2d<f32>;
@group(0) @binding(4) var bloomSampler: sampler;

// Khronos PBR Neutral, without its toe. The toe subtracts up to 0.04 from every channel, which suits
// a lit product but would roughly halve the brightness of a day side that sits around 0.05.
// What is left is the identity below kStartCompression and a shoulder that rolls highlights off
// towards white instead of clipping them.
const kStartCompression: f32 = 0.76;
const kDesaturation: f32 = 0.15;

fn tonemapNeutral(color: vec3f) -> vec3f
{
    let peak = max(color.r, max(color.g, color.b));
    if (peak < kStartCompression)
    {
        return color;
    }

    let d = 1.0 - kStartCompression;
    let newPeak = 1.0 - d * d / (peak + d - kStartCompression);
    let g = 1.0 - 1.0 / (kDesaturation * (peak - newPeak) + 1.0);
    return mix(color * (newPeak / peak), vec3f(newPeak), g);
}

// Hill's fit of the ACES RRT and sRGB ODT. The matrices are written as rows, hence color * m.
const kAcesInput = mat3x3f(
    0.59719, 0.35458, 0.04823,
    0.07600, 0.90834, 0.01566,
    0.02840, 0.13383, 0.83777);
const kAcesOutput = mat3x3f(
    1.60475, -0.53108, -0.07367,
    -0.10208, 1.10813, -0.00605,
    -0.00327, -0.07276, 1.07602);

fn tonemapAces(color: vec3f) -> vec3f
{
    let v = color * kAcesInput;
    let a = v * (v + 0.0245786) - 0.000090537;
    let b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return saturate((a / b) * kAcesOutput);
}

// Wrensch's minimal AgX, with the default look: https://iolite-engine.com/blog_posts/minimal_agx_implementation
const kAgxInset = mat3x3f(
    0.842479062253094, 0.0423282422610123, 0.0423756549057051,
    0.0784335999999992, 0.878468636469772, 0.0784336,
    0.0792237451477643, 0.0791661274605434, 0.879142973793104);
const kAgxOutset = mat3x3f(
    1.19687900512017, -0.0528968517574562, -0.0529716355144438,
    -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
    -0.0990297440797205, -0.0989611768448433, 1.15107367264116);
const kAgxMinEv: f32 = -12.47393;
const kAgxMaxEv: f32 = 4.026069;

fn tonemapAgx(color: vec3f) -> vec3f
{
    let encoded = (clamp(log2(max(kAgxInset * color, vec3f(1e-10))), vec3f(kAgxMinEv), vec3f(kAgxMaxEv)) - kAgxMinEv) / (kAgxMaxEv - kAgxMinEv);
    let x2 = encoded * encoded;
    let x4 = x2 * x2;
    let curve = 15.5 * x4 * x2 - 40.14 * x4 * encoded + 31.96 * x4 - 6.868 * x2 * encoded + 0.4298 * x2 + 0.1191 * encoded - 0.00232;
    // The curve's output is display-encoded with a 2.2 gamma; decode it, as linearToSrgb follows.
    return pow(max(kAgxOutset * curve, vec3f(0.0)), vec3f(2.2));
}

fn tonemap(color: vec3f) -> vec3f
{
    switch settings.tonemapper
    {
        case 1u: { return tonemapAces(color); }
        case 2u: { return tonemapAgx(color); }
        default: { return tonemapNeutral(color); }
    }
}

fn linearToSrgb(linear: vec3f) -> vec3f
{
    let cutoff = linear < vec3f(0.0031308);
    let higher = vec3f(1.055) * pow(linear, vec3f(1.0 / 2.4)) - vec3f(0.055);
    let lower = linear * vec3f(12.92);
    return select(higher, lower, cutoff);
}

// PCG, from Jarzynski and Olano, "Hash Functions for GPU Rendering" (JCGT 2020).
fn pcg(v: u32) -> u32
{
    let state = v * 747796405u + 2891336453u;
    let word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

// Triangular noise in (-1, 1), in units of one 8-bit step. Smooth gradients such as a skybox band
// once quantized to the surface's 8 bits per channel, and this breaks the bands up.
fn dither(texel: vec2u) -> f32
{
    let h = pcg(texel.x + pcg(texel.y));
    let a = f32(h & 0xffffu) / 65535.0;
    let b = f32(h >> 16u) / 65535.0;
    return (a + b - 1.0) / 255.0;
}

@fragment fn presentMain(in: VertexOutput) -> @location(0) vec4f
{
    // The scene and the overlay are the surface's size, so they are read texel for texel.
    let texel = vec2u(in.position.xy);
    let scene = textureLoad(sceneTexture, texel, 0).rgb;
    let bloom = textureSampleLevel(bloomTexture, bloomSampler, in.uv, 0.0).rgb * settings.bloomScale;
    let color = linearToSrgb(tonemap(mix(scene, bloom, settings.bloomStrength) * settings.exposure));
    let overlay = textureLoad(overlayTexture, texel, 0);
    return vec4f(color * (1.0 - overlay.a) + overlay.rgb + dither(texel), 1.0);
}
