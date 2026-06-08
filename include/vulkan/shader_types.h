#ifndef SHADER_TYPES_H
# define SHADER_TYPES_H

# include <stdint.h>

/* BOTH */

typedef struct s_scene_data {
    uint64_t vertex_buffer;
	uint64_t boid_buffer;
} t_scene_data;

/* COMPUTE */
typedef struct s_push_constants_compute {
    uint64_t scene;
	float    dt;
	uint32_t boid_count;
	float    speed;
} t_push_constants_compute;

/* GRAPHICS */

typedef struct s_vertex {
	float position[3];
	float uv[2];
} t_vertex;

typedef struct s_boid {
	float position[3];
	float velocity[3];
} t_boid;

typedef struct s_push_constants_graphics {
    float    mvp[16];
    uint64_t scene;
} t_push_constants_graphics;

#endif
