#ifndef BDA_H
# define BDA_H

# include "app.h"

void create_buffer(t_app *a, VkDeviceSize size, VkBufferUsageFlags usage,
                          VkMemoryPropertyFlags properties,
                          bool device_address, VkBuffer *buffer, VkDeviceMemory *memory);

void upload_buffer(t_app *a, VkDeviceMemory memory,
                          const void *data, VkDeviceSize size);

void create_spatial_hash_buffers(t_app *a);

void create_scene_buffer(t_app *a);

void upload_mesh(t_app *a);
void upload_boids(t_app *a, uint32_t count);
void upload_scene(t_app *a);

uint32_t next_pow2(uint32_t n);

#endif
