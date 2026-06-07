#ifndef SHADER_TYPES_H
# define SHADER_TYPES_H

# include <stdint.h>

typedef struct s_vertex {
	float position[3];
	float uv[2];
} t_vertex;

typedef struct s_scene_data {
    uint64_t vertex_buffer;
} t_scene_data;

typedef struct s_push_constants {
    float    mvp[16];
    uint64_t scene;
} t_push_constants;

#endif
