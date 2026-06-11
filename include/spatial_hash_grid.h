#ifndef SPATIAL_HASH_GRID_H
# define SPATIAL_HASH_GRID_H

#include <inttypes.h>

# include "boids.h"

# define MAX(a, b) ((a) > (b) ? (a) : (b))

# define SPATIAL_HASH_GRID_SLOT_FACTOR 4
# define SPATIAL_HASH_GRID_SIZE ((int)ceil(MAX(  \
		 BOID_SEPARATION_RADIUS,                 \
		 MAX(BOID_ALIGNMENT_RADIUS,              \
		 BOID_COHESION_RADIUS                    \
	))))

uint32_t next_pow2(uint32_t n);
uint32_t log2_n(uint32_t n);

#endif
