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

	int      max_dist;

	float    min_vel;
	float    max_vel;

	float    separation_radius;
	float    separation_force;

	float    alignment_radius;
	float    alignment_force;

	float    cohesion_radius;
	float    cohesion_force;

	float    avoid_border_force;
};
[[vk::push_constant]] push_constants pc;

[shader("compute")]
[numthreads(64, 1, 1)]
void compute_main(uint3 tid : SV_DispatchThreadID)
{
    scene_data *scene = (scene_data *)pc.scene;

	float dt = (float)pc.dt;

	uint32_t boid_count = (uint32_t)pc.boid_count;

	float max_dist = (int)pc.max_dist;

	float min_vel = (float)pc.min_vel;
	float max_vel = (float)pc.max_vel;

	float separation_radius = (float)pc.separation_radius;
	float separation_force = (float)pc.separation_force;

	float alignment_radius = (float)pc.alignment_radius;
	float alignment_force = (float)pc.alignment_force;

	float cohesion_radius = (float)pc.cohesion_radius;
	float cohesion_force = (float)pc.cohesion_force;

	float avoid_border_force = (float)pc.avoid_border_force;

	if (tid.x >= boid_count) return;

	boid *boids = (boid *)scene->boid_buffer;
    boid b = boids[tid.x];

	float3 separation = float3(0, 0, 0);
	float3 alignment  = float3(0, 0, 0);
	float3 cohesion   = float3(0, 0, 0);
	uint32_t align_count = 0;
	uint32_t cohesion_count = 0;

	for (uint32_t i = 0; i < boid_count; i++)
	{
		if (i == tid.x)
			continue;

		boid ob = boids[i];
		float3 diff = ob.position - b.position;
		float dist = length(diff);
		if (dist < 0.00001f)
			continue;

		float3 dir_ob = normalize(diff);

		if (dist <= separation_radius)
			separation += -dir_ob * (1.0f / dist);

		if (dist <= alignment_radius)
		{
			alignment += normalize(ob.velocity);
			align_count += 1;
		}

		if (dist <= cohesion_radius)
		{
			cohesion += ob.position;
			cohesion_count += 1;
		}
	}

	b.velocity += separation * separation_force * dt;

	if (align_count > 0)
		b.velocity += normalize(alignment) * alignment_force * dt;
	
	if (cohesion_count > 0)
	{
		float3 center_group = cohesion / cohesion_count;
		float3 diff = center_group - b.position;
		b.velocity += normalize(diff) * cohesion_force * dt;
	}

	// avoid border
	float3 center = float3(0.0f, 0.0f, 0.0f);
	float3 diff = center - b.position;
	float dist = length(diff);
	float dist_passed_border = dist - max_dist;
	if (dist_passed_border > 0)
		b.velocity += normalize(diff) * avoid_border_force * dist_passed_border * dt;
	
	if (b.velocity.y < 0)
		b.velocity *= exp(0.25f * dt);

	float speed = length(b.velocity);
	if (speed < 0.00001f)
    	b.velocity = float3(0, 0, 1) * min_vel;
	else if (speed < min_vel)
		b.velocity = (b.velocity / speed) * min_vel;
	else if (speed > max_vel)
		b.velocity = (b.velocity / speed) * max_vel;

    b.position += b.velocity * dt;

    boids[tid.x] = b;
}
