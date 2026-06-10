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

void upload_buffer_through_staging(t_app *a, void *data, VkDeviceSize size, 
                            VkBufferUsageFlags usage, bool device_address,
                            VkBuffer *dst_buffer, VkDeviceMemory *dst_memory)
{
	/* temp staging buffer to put data on the gpu */
    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
    create_buffer(a, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			false, &staging_buffer, &staging_memory);

    upload_buffer(a, staging_memory, data, size);

	/* actual gpu buffer we will use */
    create_buffer(a, size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			device_address, dst_buffer, dst_memory);

	/* one-time command buffer to copy the data from the staging buffer */
    VkCommandBufferAllocateInfo cbai = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = a->cmd_pool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer copy_cmd;
    vkAllocateCommandBuffers(a->device, &cbai, &copy_cmd);

    VkCommandBufferBeginInfo cbbi = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(copy_cmd, &cbbi);

    VkBufferCopy copy_region = { .srcOffset = 0, .dstOffset = 0, .size = size };
    vkCmdCopyBuffer(copy_cmd, staging_buffer, *dst_buffer, 1, &copy_region);
    vkEndCommandBuffer(copy_cmd);

    VkSubmitInfo submit_info = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &copy_cmd,
    };
    vkQueueSubmit(a->graphics_q, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(a->graphics_q);

    vkFreeCommandBuffers(a->device, a->cmd_pool, 1, &copy_cmd);
    vkDestroyBuffer(a->device, staging_buffer, NULL);
    vkFreeMemory(a->device, staging_memory, NULL);
}

void upload_mesh(t_app *a)
{
    Mesh *m = &a->mesh;

    /* vertex buffer */
    VkDeviceSize vert_size = sizeof(t_vertex) * m->vertex_count;
	upload_buffer_through_staging(a, m->vertices, vert_size,
                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  true, &a->vertex_buffer, &a->vertex_memory);

    /* index buffer */
    VkDeviceSize idx_size = sizeof(uint32_t) * m->index_count;
	upload_buffer_through_staging(a, m->indices, idx_size,
                  VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                  false, &a->index_buffer, &a->index_memory);
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
	upload_buffer_through_staging(a, boids, boids_size,
                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  true, &a->boid_buffer, &a->boid_memory);
	free(boids);
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
