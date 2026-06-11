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
	uint64_t slot_boid_count_buffer;
	uint64_t slot_offset_buffer;

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

	uint32_t k = tid.x * stride * 2;
	uint32_t left = k + stride - 1;
	uint32_t right = k + stride * 2 - 1;

	if (right >= slot_count_padded) return;

	slot_offset[right] += slot_offset[left];
}
