// The output data flying out of the Vertex shader into the Fragment shader
struct VertexStageOutput {
    float4 position : SV_Position;
    float3 color    : COLOR;
};

[shader("vertex")]
VertexStageOutput vertexMain(uint vertexID : SV_VertexID) {
    VertexStageOutput output;

    // Hardcode the 3 corners of the triangle for a quick test
    float2 positions[3] = {
        float2(0.0, -0.5),
        float2(0.5,  0.5),
        float2(-0.5, 0.5)
    };

    float3 colors[3] = {
        float3(1.0, 0.0, 0.0), // Red
        float3(0.0, 1.0, 0.0), // Green
        float3(0.0, 0.0, 1.0)  // Blue
    };

    output.position = float4(positions[vertexID], 0.0, 1.0);
    output.color = colors[vertexID];
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexStageOutput input) : SV_Target {
    // Interpolate the colors across the surface of the triangle
    return float4(input.color, 1.0);
}
