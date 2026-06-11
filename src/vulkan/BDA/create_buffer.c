/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   create_buffer.c                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aginiaux <aginiaux@student.42lyon.fr>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/10 19:09:52 by aginiaux          #+#    #+#             */
/*   Updated: 2026/06/11 14:32:38 by aginiaux         ###   ########lyon.fr   */
/*                                                                            */
/* ************************************************************************** */

#include "app.h"
#include "spatial_hash_grid.h"
#include "vulkan/BDA.h"

/* ================================================================== */
/*  Buffer Device Address — small scene-data buffer
 *
 *  We allocate a tiny buffer with SHADER_DEVICE_ADDRESS usage so the
 *  GPU can access it via a raw 64-bit pointer.  The address is
 *  retrieved with vkGetBufferDeviceAddress and would be pushed to a
 *  shader via a push constant.
 * ================================================================== */
static uint32_t find_memory_type(t_app *a, uint32_t filter,
		VkMemoryPropertyFlags flags)
{
	VkPhysicalDeviceMemoryProperties mp;
	vkGetPhysicalDeviceMemoryProperties(a->physical, &mp);
	for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
		if ((filter & (1u << i)) &&
				(mp.memoryTypes[i].propertyFlags & flags) == flags)
			return i;
	fputs("No suitable memory type\n", stderr);
	exit(1);
}

void create_buffer(t_app *a, VkDeviceSize size, VkBufferUsageFlags usage,
                          VkMemoryPropertyFlags properties,
                          bool device_address, VkBuffer *buffer, VkDeviceMemory *memory)
{
    VkBufferCreateInfo bi = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = size,
        .usage       = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VK_CHECK(vkCreateBuffer(a->device, &bi, NULL, buffer));

    VkMemoryRequirements mr;
    vkGetBufferMemoryRequirements(a->device, *buffer, &mr);

    VkMemoryAllocateFlagsInfo maf = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
    };

	// TEMP DEBUG
	properties |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    VkMemoryAllocateInfo mai = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = device_address ? &maf : NULL,
        .allocationSize  = mr.size,
        .memoryTypeIndex = find_memory_type(a, mr.memoryTypeBits, properties),
    };
    VK_CHECK(vkAllocateMemory(a->device, &mai, NULL, memory));
    VK_CHECK(vkBindBufferMemory(a->device, *buffer, *memory, 0));
}

void create_spatial_hash_buffers(t_app *a)
{
	/* boid slot */
	create_buffer(a, sizeof(uint32_t) * a->boid_count,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT          |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			true, &a->boid_slot_buffer, &a->boid_slot_memory);

	VkBufferDeviceAddressInfo bdai = {
        .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = a->boid_slot_buffer,
    };
    a->boid_slot_address = vkGetBufferDeviceAddress(a->device, &bdai);

	/* slot count */
	/* power of 2 for blelloch prefix sum algo */
	a->slot_count = a->boid_count * SPATIAL_HASH_GRID_SLOT_FACTOR;
	a->slot_count_padded = next_pow2(a->slot_count);
	create_buffer(a, sizeof(uint32_t) * a->slot_count_padded,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT          |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT   |
			VK_BUFFER_USAGE_2_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			true, &a->slot_boid_count_buffer, &a->slot_boid_count_memory);

	bdai.buffer = a->slot_boid_count_buffer;
    a->slot_boid_count_address = vkGetBufferDeviceAddress(a->device, &bdai);

	/* slot offset (same buffer as slot count) */
	a->slot_offset_address = a->slot_boid_count_address;
}

void create_scene_buffer(t_app *a)
{
    create_buffer(a, sizeof(t_scene_data),
                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  true, &a->scene_buffer, &a->scene_memory);

    VkBufferDeviceAddressInfo bdai = {
        .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = a->scene_buffer,
    };
    a->scene_address = vkGetBufferDeviceAddress(a->device, &bdai);
}
