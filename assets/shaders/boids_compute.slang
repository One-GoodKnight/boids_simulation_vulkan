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
    uint64_t scene;
	float    dt;
    uint32_t boid_count;
	float    speed;
};
[[vk::push_constant]] push_constants pc;

[shader("compute")]
[numthreads(64, 1, 1)]
void compute_main(uint3 tid : SV_DispatchThreadID)
{
	uint32_t boid_count = (uint32_t)pc.boid_count;

	if (tid.x >= boid_count) return;

	float dt = (float)pc.dt;
    scene_data *scene = (scene_data *)pc.scene;
    boid *boids = (boid *)scene->boid_buffer;
    boid b = boids[tid.x];
	float speed = (float)pc.speed;

    b.position += 0;// b.velocity * speed * dt;

    boids[tid.x] = b;
}
