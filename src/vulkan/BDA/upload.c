#include <stdlib.h>

#include "vulkan/BDA.h"
#include "app.h"
#include "boids.h"

void upload_buffer(t_app *a, VkDeviceMemory memory,
                          const void *data, VkDeviceSize size)
{
    void *mapped;
    VK_CHECK(vkMapMemory(a->device, memory, 0, size, 0, &mapped));
    memcpy(mapped, data, size);
    vkUnmapMemory(a->device, memory);
}

void upload_mesh(t_app *a)
{
    Mesh *m = &a->mesh;

    /* vertex buffer */
    VkDeviceSize vert_size = sizeof(t_vertex) * m->vertex_count;
    create_buffer(a, vert_size,
                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  true, &a->vertex_buffer, &a->vertex_memory);
    upload_buffer(a, a->vertex_memory, m->vertices, vert_size);

    /* index buffer */
    VkDeviceSize idx_size = sizeof(uint32_t) * m->index_count;
    create_buffer(a, idx_size,
                  VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                  false, &a->index_buffer, &a->index_memory);
    upload_buffer(a, a->index_memory, m->indices, idx_size);
}

void upload_boids(t_app *a, uint32_t count)
{
	a->boid_count = count;

	t_boid *boids = malloc(sizeof(t_boid) * count);
	for (uint32_t i = 0; i < count; i++)
	{
		boids[i].position[0] = ((float)rand() / RAND_MAX) * MAX_DISTANCE - MAX_DISTANCE / 2;
		boids[i].position[1] = ((float)rand() / RAND_MAX) * MAX_DISTANCE - MAX_DISTANCE / 2;
		boids[i].position[2] = ((float)rand() / RAND_MAX) * MAX_DISTANCE - MAX_DISTANCE / 2;
		boids[i].velocity[0] = ((float)rand() / RAND_MAX) * BOID_START_VEL - BOID_START_VEL / 2;
		boids[i].velocity[1] = ((float)rand() / RAND_MAX) * BOID_START_VEL - BOID_START_VEL / 2;
		boids[i].velocity[2] = ((float)rand() / RAND_MAX) * BOID_START_VEL - BOID_START_VEL / 2;
	}

	/* boids buffer */
    VkDeviceSize boids_size = sizeof(t_boid) * count;
    create_buffer(a, boids_size,
                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  true, &a->boid_buffer, &a->boid_memory);
    upload_buffer(a, a->boid_memory, boids, boids_size);
}

void upload_scene(t_app *a)
{
	VkBufferDeviceAddressInfo bdai = {
        .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = a->vertex_buffer,
    };
    VkDeviceAddress vertex_address = vkGetBufferDeviceAddress(a->device, &bdai);

	bdai.buffer = a->boid_buffer;
    VkDeviceAddress boid_address = vkGetBufferDeviceAddress(a->device, &bdai);

	t_scene_data scene_data = {
		vertex_address,
		boid_address
	};
    upload_buffer(a, a->scene_memory, &scene_data, sizeof(t_scene_data));
}
