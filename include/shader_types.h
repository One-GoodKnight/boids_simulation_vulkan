#ifndef SHADER_TYPES_H
# define SHADER_TYPES_H

# include <stdint.h>
# include <stdbool.h>

/* BOTH */

typedef struct s_scene_data {
    uint64_t vertex_buffer;
	uint64_t boid_buffer;
} t_scene_data;

typedef struct s_boid {
	float position[3];
	float velocity[3];
} t_boid;

/* SPATIAL HASH GRID */
typedef struct s_push_constants_compute_spatial_hash {
    uint64_t scene;

	uint64_t boid_slot_buffer;

	uint32_t boid_count;

	uint32_t slot_count;
	float    cell_size;
} t_push_constants_compute_spatial_hash;

/* COMPUTE */
typedef struct s_push_constants_compute {
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
} t_push_constants_compute;

/* GRAPHICS */

typedef struct s_vertex {
	float position[3];
	float normal[3];
	float uv[2];
} t_vertex;

typedef struct s_push_constants_graphics {
    float    mvp[16];
    uint64_t scene;

	float    outline_thickness;
} t_push_constants_graphics;

#endif
