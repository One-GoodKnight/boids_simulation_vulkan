struct vertex {
    float3 position;
    float2 uv;
};

struct boid {
	float3 position;
	float3 velocity;
};

struct scene_data {
    uint64_t vertex_buffer;
	uint64_t boid_buffer;
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
vertex_stage_output vertex_main(
	uint vertex_id : SV_VertexID,
	uint instance_id : SV_InstanceID
) {
	scene_data *scene = (scene_data *)pc.scene;

    vertex *vertices = (vertex *)scene->vertex_buffer;
    vertex v = vertices[vertex_id];

	boid   *boids = (boid *)scene->boid_buffer;
	boid   b = boids[instance_id];

	float3 forward = normalize(b.velocity);
	float3 up = float3(0, 1, 0);
	float3 right = normalize(cross(up, forward));
	up = cross(forward, right); // local up

	float3x3 rot = float3x3(
		right.x,   up.x,   forward.x,
		right.y,   up.y,   forward.y,
		right.z,   up.z,   forward.z
	);
	float3 world_pos = mul(rot, v.position) + b.position;

    vertex_stage_output output;
    output.position = mul(pc.mvp, float4(world_pos, 1.0f));
    output.uv       = v.uv;
    return output;
}

[shader("fragment")]
float4 fragment_main(vertex_stage_output input) : SV_Target {
    return float4(1.0f, 0.0f, 0.0f, 1.0f);
}
