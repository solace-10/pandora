// Included by fullscreen passes, which draw it with Draw(3) and no vertex buffer.
struct VertexOutput
{
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f
};

// One triangle large enough to cover the target.
@vertex fn vertexMain(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput
{
    let xy = vec2f(f32((vertexIndex << 1u) & 2u), f32(vertexIndex & 2u));
    var out: VertexOutput;
    out.position = vec4f(xy * 2.0 - 1.0, 0.0, 1.0);
    out.uv = vec2f(xy.x, 1.0 - xy.y);
    return out;
}
