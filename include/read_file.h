#ifndef READ_FILE_H
# define READ_FILE_H

# include <stdlib.h>
# include <stdint.h>

uint32_t* load_spirv_file_c(const char* filename, size_t* out_size);

#endif
