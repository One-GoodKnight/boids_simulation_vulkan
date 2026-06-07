#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

uint32_t* load_spirv_file(const char* filename, size_t* out_size)
{
    // Read-Binary
    FILE* file = fopen(filename, "rb");
    if (!file)
	{
		fprintf(stderr, "File %s not found.\n", filename);
		exit(1);
	}

    fseek(file, 0, SEEK_END);
    size_t bytes = ftell(file);
    fseek(file, 0, SEEK_SET);

    uint32_t* buffer = (uint32_t*)malloc(bytes);
    if (!buffer) {
        fclose(file);
		exit(1);
    }

    size_t read_bytes = fread(buffer, 1, bytes, file);
    fclose(file);

    if (read_bytes != bytes) {
        fprintf(stderr, "Failed to read entire file: %s\n", filename);
        free(buffer);
		exit(1);
    }

    *out_size = bytes;
    return buffer;
}
