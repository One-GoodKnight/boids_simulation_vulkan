#ifndef READ_FILE_H
# define READ_FILE_H

# include <stdlib.h>
# include <stdint.h>

# include "geometry.h"

Mesh 		load_mesh_from_gltf_file(const char *path);
uint32_t*	load_spirv_file(const char* filename, size_t* out_size);

#endif
