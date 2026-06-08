#ifndef GEOMETRY_H
# define GEOMETRY_H

# include <stdint.h>

# include "vulkan/shader_types.h"

typedef struct {
	t_vertex *vertices;
	uint32_t  vertex_count;
	uint32_t *indices;
	uint32_t  index_count;
} Mesh;

#endif
