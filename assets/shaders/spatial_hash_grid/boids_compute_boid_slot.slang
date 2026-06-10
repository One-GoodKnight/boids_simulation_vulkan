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

	uint64_t boid_slot_buffer;

	uint32_t boid_count;

	uint32_t slot_count;
	float cell_size;
};
[[vk::push_constant]] push_constants pc;

uint hash_cell(int3 cell, uint32_t slot_count)
{
    uint x = (uint)cell.x * 2185031351u;
    uint y = (uint)cell.y * 4232452283u;
    uint z = (uint)cell.z * 3905402947u;
    return (x ^ y ^ z) % slot_count;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void compute_main(uint3 tid : SV_DispatchThreadID)
{
	scene_data *scene = (scene_data *)pc.scene;
	boid *boids = (boid *)scene->boid_buffer;

	uint32_t *boid_slot = (uint32_t *)pc.boid_slot_buffer;

	uint32_t boid_count = (uint32_t)pc.boid_count;

	uint32_t slot_count = (uint32_t)pc.slot_count;
	uint32_t cell_size = (uint32_t)pc.cell_size;

	if (tid.x >= boid_count) return;

	boid b = boids[tid.x];

	int cell_x = (int)floor(b.position.x / cell_size);
	int cell_y = (int)floor(b.position.y / cell_size);
	int cell_z = (int)floor(b.position.z / cell_size);

	boid_slot[tid.x] = hash_cell(int3(cell_x, cell_y, cell_z), slot_count);
}
