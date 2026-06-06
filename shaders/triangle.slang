struct Vertex {
    float4 position;
    float4 color;
};

struct PushConstants {
    uint64_t vertex_buffer;
};

[[vk::push_constant]] PushConstants pc;

struct VertexStageOutput {
    float4 position : SV_Position;
    float4 color    : COLOR;
};

[shader("vertex")]
VertexStageOutput vertexMain(uint vertexID : SV_VertexID)
{
    Vertex *vertices = (Vertex *)pc.vertex_buffer;
    Vertex v = vertices[vertexID];

    VertexStageOutput output;
    output.position = float4(v.position);
    output.color    = v.color;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexStageOutput input) : SV_Target {
    return float4(input.color);
}
