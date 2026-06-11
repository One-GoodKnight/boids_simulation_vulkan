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
	uint32_t *slot_offset = (uint32_t *)pc.slot_offset_buffer;

	uint32_t slot_count_padded = (uint32_t)pc.slot_count_padded;

	uint32_t stride = (uint32_t)pc.stride;

	uint64_t k = (uint64_t)tid.x * (uint64_t)stride * 2;
	uint64_t left = k + (uint64_t)stride - 1;
	uint64_t right = k + (uint64_t)stride * 2 - 1;

	if (right >= (uint64_t)slot_count_padded) return;

	uint32_t temp = slot_offset[(uint32_t)left];
	slot_offset[left] = slot_offset[(uint32_t)right];
	slot_offset[right] += temp;
}
