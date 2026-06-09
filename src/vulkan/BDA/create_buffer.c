#include "app.h"

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
    VkMemoryAllocateInfo mai = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = device_address ? &maf : NULL,
        .allocationSize  = mr.size,
        .memoryTypeIndex = find_memory_type(a, mr.memoryTypeBits, properties),
    };
    VK_CHECK(vkAllocateMemory(a->device, &mai, NULL, memory));
    VK_CHECK(vkBindBufferMemory(a->device, *buffer, *memory, 0));
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
