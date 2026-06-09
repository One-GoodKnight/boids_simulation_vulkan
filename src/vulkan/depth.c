#include "vulkan/depth.h"

void create_depth_buffer(t_app *a)
{
    a->depth_format = VK_FORMAT_D32_SFLOAT;

    VkImageCreateInfo ci = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = a->depth_format,
        .extent        = { a->sc_extent.width, a->sc_extent.height, 1 },
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VK_CHECK(vkCreateImage(a->device, &ci, NULL, &a->depth_image));

    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(a->device, a->depth_image, &mem_req);

    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(a->physical, &mem_props);

    uint32_t mem_type = UINT32_MAX;
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((mem_req.memoryTypeBits & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        { mem_type = i; break; }
    }

    VkMemoryAllocateInfo ai = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = mem_req.size,
        .memoryTypeIndex = mem_type,
    };
    VK_CHECK(vkAllocateMemory(a->device, &ai, NULL, &a->depth_memory));
    vkBindImageMemory(a->device, a->depth_image, a->depth_memory, 0);

    VkImageViewCreateInfo vci = {
        .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image    = a->depth_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format   = a->depth_format,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .levelCount = 1,
            .layerCount = 1,
        },
    };
    VK_CHECK(vkCreateImageView(a->device, &vci, NULL, &a->depth_view));
}

void destroy_depth_resources(t_app *a)
{
    vkDestroyImageView(a->device, a->depth_view,   NULL);
    vkDestroyImage    (a->device, a->depth_image,  NULL);
    vkFreeMemory      (a->device, a->depth_memory, NULL);
}
