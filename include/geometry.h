#ifndef GEOMETRY_H
# define GEOMETRY_H

# include <stdint.h>

typedef struct {
	float position[3];
	float uv[2];
} Vertex;

typedef struct {
	Vertex   *vertices;
	uint32_t  vertex_count;
	uint32_t *indices;
	uint32_t  index_count;
} Mesh;

#endif
