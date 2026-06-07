struct vertex {
    float3 position;
    float2 uv;
};

struct scene_data {
    uint64_t vertex_buffer;
};

struct push_constants {
    float4x4 mvp;
    uint64_t scene;   /* address of scene_data */
};

[[vk::push_constant]] push_constants pc;

struct vertex_stage_output {
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

[shader("vertex")]
vertex_stage_output vertex_main(uint vertex_id : SV_VertexID)
{
	scene_data *scene = (scene_data *)pc.scene;
    vertex *vertices = (vertex *)scene->vertex_buffer;
    vertex v = vertices[vertex_id];

    vertex_stage_output output;
    output.position = mul(pc.mvp, float4(v.position, 1.0f));
    output.uv       = v.uv;
    return output;
}

[shader("fragment")]
float4 fragment_main(vertex_stage_output input) : SV_Target {
    return float4(1.0f, 0.0f, 0.0f, 1.0f);
}
