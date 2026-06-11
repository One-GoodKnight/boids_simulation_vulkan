struct boid {
    float3 position;
    float3 velocity;
};
struct scene_data {
    uint64_t vertex_buffer;
    uint64_t boid_buffer;
};
struct push_constants {
    uint64_t debug_buffer;
    uint64_t scene;

	uint64_t boid_slot_buffer;
	uint64_t slot_boid_count_buffer;
	uint64_t slot_offset_buffer;
	uint64_t slot_cursor_buffer;
	uint64_t sorted_boid_buffer;

	uint32_t boid_count;

	uint32_t slot_count;
	uint32_t slot_count_padded;
	float    cell_size;

	uint32_t stride;
};
[[vk::push_constant]] push_constants pc;

[shader("compute")]
[numthreads(64, 1, 1)]
void compute_main(uint3 tid : SV_DispatchThreadID)
{
	uint32_t *debug = (uint32_t *)pc.debug_buffer;

	uint32_t *boid_slot = (uint32_t *)pc.boid_slot_buffer;
	uint32_t *slot_cursor = (uint32_t *)pc.slot_cursor_buffer;
	uint32_t *sorted_boid = (uint32_t *)pc.sorted_boid_buffer;

	uint32_t boid_count = (uint32_t)pc.boid_count;

	if (tid.x >= boid_count) return;

	uint32_t slot = boid_slot[tid.x];
	uint32_t pos;
	InterlockedAdd(slot_cursor[slot], 1, pos);

	sorted_boid[pos] = tid.x;
}
