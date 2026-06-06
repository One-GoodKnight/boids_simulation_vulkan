struct Vertex {
    float3 position;
    float3 color;
};

struct PushConstants {
    uint64_t vertex_buffer;
};

[[vk::push_constant]] PushConstants pc;

struct VertexStageOutput {
    float4 position : SV_Position;
    float3 color    : COLOR;
};

[shader("vertex")]
VertexStageOutput vertexMain(uint vertexID : SV_VertexID)
{
    Vertex *vertices = (Vertex *)pc.vertex_buffer;
    Vertex v = vertices[vertexID];

    VertexStageOutput output;
    output.position = float4(v.position, 1.0);
    output.color    = v.color;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexStageOutput input) : SV_Target {
    return float4(input.color, 1.0);
}
