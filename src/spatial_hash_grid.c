#include <stdio.h>
#include <stdlib.h>

#include "spatial_hash_grid.h"

uint32_t next_pow2(uint32_t n)
{
    n--;
	/* fill all the bits to the right of the first 1 */
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
	/* 0011111 -> 0100000, our power of 2 */
    return n + 1;
}

uint32_t log2_n(uint32_t n)
{
	if (n == 0)
	{
		fprintf(stderr, "log2(0) undefined\n");
		exit(1);
	}

	// ...00100000 -> founds that first 1 is at index 5 and returns
	int count = 31;
	while (n >> count == 0)
		count--;
	return count;
}
