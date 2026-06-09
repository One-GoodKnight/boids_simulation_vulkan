struct vertex {
    float3 position;
	float3 normal;
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
    uint64_t scene;

	float    outline_thickness;
};

[[vk::push_constant]] push_constants pc;

struct vertex_stage_output {
    float4 position : SV_Position;

	nointerpolation float3 velocity : TEXCOORD0;
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
	float3 up = float3(0, 1, 0); // global up
	float3 right = normalize(cross(up, forward));
	up = cross(forward, right); // local up

	float3x3 rot = float3x3(
		right.x,   up.x,   forward.x,
		right.y,   up.y,   forward.y,
		right.z,   up.z,   forward.z
	);

	float3 world_pos    = mul(rot, v.position) + b.position;
	float3 world_normal = mul(rot, v.normal);

	float4 clip_pos    = mul(pc.mvp, float4(world_pos, 1.0f));
	float4 clip_normal = mul(pc.mvp, float4(world_normal, 0.0f));

	if (pc.outline_thickness > 0.0f)
	{
		float2 dir = normalize(clip_normal.xy);
		clip_pos.xy += dir * pc.outline_thickness;
	}

    vertex_stage_output output;
    output.position = clip_pos;
    output.velocity = b.velocity;
    return output;
}

[shader("fragment")]
float4 fragment_main(
	vertex_stage_output input
) : SV_Target {
	if (pc.outline_thickness > 0.0f)
    	return float4(0.0f, 0.0f, 0.0f, 1.0f);

	float3 dir = normalize(input.velocity);

	float3 color = (dir + float3(1.0f, 1.0f, 1.0f));

	float3 dark_color = color * 0.45f;

    return float4(dark_color, 1.0f);
}
