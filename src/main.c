#include <SDL3/SDL_events.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "app.h"
#include "camera.h"
#include "load_files.h"
#include "shader_types.h"

/* ------------------------------------------------------------------- */
/*  Error helper                                                       */
/* ------------------------------------------------------------------- */
#define VK_CHECK(x)                                                     \
	do {                                                                \
		VkResult _r = (x);                                            	\
		if (_r != VK_SUCCESS) {                                       	\
			fprintf(stderr, "Vulkan error %d at %s:%d\n",            	\
					_r, __FILE__, __LINE__);                          	\
			exit(1);                                                  	\
		}                                                            	\
	} while (0)

/* ================================================================== */
/*  Instance  (Vulkan 1.3 requested)                                  */
/* ================================================================== */
static void create_instance(t_app *a)
{
	uint32_t sdl_ext_count = 0;
	const char * const *sdl_exts =
		SDL_Vulkan_GetInstanceExtensions(&sdl_ext_count);
	if (!sdl_exts) {
		fprintf(stderr, "SDL_Vulkan_GetInstanceExtensions: %s\n",
				SDL_GetError());
		exit(1);
	}

	VkApplicationInfo app_info = {
		.sType      = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.apiVersion = VK_API_VERSION_1_3,   /* request 1.3 */
	};

	VkInstanceCreateInfo ci = {
		.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo        = &app_info,
		.enabledExtensionCount   = sdl_ext_count,
		.ppEnabledExtensionNames = sdl_exts,
#ifdef DEBUG
		.enabledLayerCount       = 1,
		.ppEnabledLayerNames     = (const char *[]){ "VK_LAYER_KHRONOS_validation" },
#endif
	};

	VK_CHECK(vkCreateInstance(&ci, NULL, &a->instance));
}

/* ================================================================== */
/*  Physical device selection                                          */
/* ================================================================== */
static void pick_physical_device(t_app *a)
{
	uint32_t count = 0;
	vkEnumeratePhysicalDevices(a->instance, &count, NULL);
	if (!count) { fputs("No Vulkan GPUs found\n", stderr); exit(1); }

	VkPhysicalDevice *devs = malloc(sizeof(*devs) * count);
	vkEnumeratePhysicalDevices(a->instance, &count, devs);

	for (uint32_t i = 0; i < count; i++) {
		/* check Vulkan 1.3 support */
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(devs[i], &props);
		if (props.apiVersion < VK_API_VERSION_1_3) continue;

		/* find graphics + present queues */
		uint32_t qc = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(devs[i], &qc, NULL);
		VkQueueFamilyProperties *qp = malloc(sizeof(*qp) * qc);
		vkGetPhysicalDeviceQueueFamilyProperties(devs[i], &qc, qp);

		int gfx = -1, pres = -1;
		for (int j = 0; (uint32_t)j < qc; j++) {
			if (qp[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)
				gfx = j;
			VkBool32 ps = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(devs[i], j, a->surface, &ps);
			if (ps)
				pres = j;
		}
		free(qp);

		if (gfx >= 0 && pres >= 0) {
			a->physical        = devs[i];
			a->graphics_family = (uint32_t)gfx;
			a->present_family  = (uint32_t)pres;
			free(devs);

			VkPhysicalDeviceProperties properties;
			vkGetPhysicalDeviceProperties(a->physical, &properties);
			printf("=========================================\n");
			printf(" Active Vulkan GPU: %s\n", properties.deviceName);
			printf(" Driver Version:    %d.%d.%d\n", 
			   VK_VERSION_MAJOR(properties.driverVersion),
			   VK_VERSION_MINOR(properties.driverVersion),
			   VK_VERSION_PATCH(properties.driverVersion));
			printf("=========================================\n");

			return;
		}
	}
	free(devs);
	fputs("No suitable Vulkan 1.3 device found\n", stderr);
	exit(1);
}

/* ================================================================== */
/*  Logical device
 * ================================================================== */
static void create_device(t_app *a)
{
	/* ---- feature structs (chained) -------------------------------- */

	VkPhysicalDeviceFeatures2 base_features = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .features.shaderInt64 = VK_TRUE, /* BDA pointers */
		.features.fillModeNonSolid = VK_TRUE, /* Wireframe */
    };

	/* Vulkan 1.1: shader draw params */
	VkPhysicalDeviceVulkan11Features feat11 = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
		.pNext = &base_features,

		.shaderDrawParameters = VK_TRUE,
	};

	/* Vulkan 1.2: buffer device address + descriptor indexing */
	VkPhysicalDeviceVulkan12Features feat12 = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
		.pNext = &feat11,

		/* Buffer Device Address */
		.bufferDeviceAddress = VK_TRUE,
		.scalarBlockLayout = VK_TRUE,

		/* Descriptor Indexing ("bindless") */
		.descriptorIndexing                                = VK_TRUE,
		.shaderSampledImageArrayNonUniformIndexing         = VK_TRUE,
		.runtimeDescriptorArray                            = VK_TRUE,
		.descriptorBindingVariableDescriptorCount          = VK_TRUE,
		.descriptorBindingPartiallyBound                   = VK_TRUE,
		.descriptorBindingSampledImageUpdateAfterBind      = VK_TRUE,
		.descriptorBindingUpdateUnusedWhilePending         = VK_TRUE,
	};

	/* Vulkan 1.3: dynamic rendering + synchronization2 */
	VkPhysicalDeviceVulkan13Features feat13 = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
		.pNext = &feat12,

		/* Dynamic Rendering — no more VkRenderPass / VkFramebuffer */
		.dynamicRendering = VK_TRUE,

		/* Synchronization2 — cleaner barriers and submit */
		.synchronization2 = VK_TRUE,
	};

	/* ---- queues --------------------------------------------------- */
	float prio = 1.0f;
	uint32_t families[2] = { a->graphics_family, a->present_family };
	uint32_t fam_count   = (a->graphics_family == a->present_family) ? 1 : 2;

	VkDeviceQueueCreateInfo qcis[2] = {0};
	for (uint32_t i = 0; i < fam_count; i++) {
		qcis[i].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		qcis[i].queueFamilyIndex = families[i];
		qcis[i].queueCount       = 1;
		qcis[i].pQueuePriorities = &prio;
	}

	const char *dev_exts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	VkDeviceCreateInfo ci = {
		.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext                   = &feat13,   /* feature chain */
		.queueCreateInfoCount    = fam_count,
		.pQueueCreateInfos       = qcis,
		.enabledExtensionCount   = 1,
		.ppEnabledExtensionNames = dev_exts,
	};

	VK_CHECK(vkCreateDevice(a->physical, &ci, NULL, &a->device));

	vkGetDeviceQueue(a->device, a->graphics_family, 0, &a->graphics_q);
	vkGetDeviceQueue(a->device, a->present_family,  0, &a->present_q);

	/* ---- load Vulkan 1.3 function pointers ----------------------- */
#define LOAD(name) \
	a->fn_##name = (PFN_vk##name) \
	vkGetDeviceProcAddr(a->device, "vk" #name); \
	if (!a->fn_##name) { \
		fprintf(stderr, "vk" #name " not found\n"); exit(1); }

	LOAD(CmdBeginRendering)
	LOAD(CmdEndRendering)
	LOAD(CmdPipelineBarrier2)
	LOAD(QueueSubmit2)
#undef LOAD
}

/* ================================================================== */
/*  Swapchain (no render pass / framebuffers needed with dynrender)    */
/* ================================================================== */
static VkSurfaceFormatKHR choose_format(t_app *a)
{
	uint32_t n = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(a->physical, a->surface, &n, NULL);
	VkSurfaceFormatKHR *fmts = malloc(sizeof(*fmts) * n);
	vkGetPhysicalDeviceSurfaceFormatsKHR(a->physical, a->surface, &n, fmts);
	VkSurfaceFormatKHR chosen = fmts[0];
	for (uint32_t i = 0; i < n; i++) {
		if (fmts[i].format     == VK_FORMAT_B8G8R8A8_SRGB &&
				fmts[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			chosen = fmts[i]; break;
		}
	}
	free(fmts);
	return chosen;
}

static VkPresentModeKHR choose_present_mode(t_app *a)
{
	uint32_t n = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(
			a->physical, a->surface, &n, NULL);
	VkPresentModeKHR *modes = malloc(sizeof(*modes) * n);
	vkGetPhysicalDeviceSurfacePresentModesKHR(
			a->physical, a->surface, &n, modes);
	VkPresentModeKHR chosen = VK_PRESENT_MODE_FIFO_KHR;
	for (uint32_t i = 0; i < n; i++) {
		if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			chosen = modes[i]; break;
		}
	}
	free(modes);
	return chosen;
}

static void create_swapchain(t_app *a)
{
	VkSurfaceCapabilitiesKHR caps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
			a->physical, a->surface, &caps);

	VkSurfaceFormatKHR fmt  = choose_format(a);
	VkPresentModeKHR   mode = choose_present_mode(a);

	VkExtent2D ext;
	if (caps.currentExtent.width != UINT32_MAX) {
		ext = caps.currentExtent;
	} else {
		int w, h;
		SDL_GetWindowSizeInPixels(a->window, &w, &h);
		ext.width  = (uint32_t)w;
		ext.height = (uint32_t)h;
		if (ext.width  < caps.minImageExtent.width)  ext.width  = caps.minImageExtent.width;
		if (ext.width  > caps.maxImageExtent.width)  ext.width  = caps.maxImageExtent.width;
		if (ext.height < caps.minImageExtent.height) ext.height = caps.minImageExtent.height;
		if (ext.height > caps.maxImageExtent.height) ext.height = caps.maxImageExtent.height;
	}

	uint32_t img_count = caps.minImageCount + 1;
	if (caps.maxImageCount > 0 && img_count > caps.maxImageCount)
		img_count = caps.maxImageCount;

	uint32_t families[2] = { a->graphics_family, a->present_family };

	VkSwapchainCreateInfoKHR ci = {
		.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface          = a->surface,
		.minImageCount    = img_count,
		.imageFormat      = fmt.format,
		.imageColorSpace  = fmt.colorSpace,
		.imageExtent      = ext,
		.imageArrayLayers = 1,
		/* TRANSFER_DST needed if you blit to it; COLOR_ATTACHMENT for rendering */
		.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.preTransform     = caps.currentTransform,
		.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode      = mode,
		.clipped          = VK_TRUE,
		.oldSwapchain     = VK_NULL_HANDLE,
	};

	if (a->graphics_family != a->present_family) {
		ci.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
		ci.queueFamilyIndexCount = 2;
		ci.pQueueFamilyIndices   = families;
	} else {
		ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	VK_CHECK(vkCreateSwapchainKHR(a->device, &ci, NULL, &a->swapchain));
	a->sc_format = fmt.format;
	a->sc_extent = ext;

	vkGetSwapchainImagesKHR(a->device, a->swapchain, &a->sc_image_count, NULL);
	a->sc_images  = malloc(sizeof(VkImage)       * a->sc_image_count);
	a->sc_views   = malloc(sizeof(VkImageView)   * a->sc_image_count);
	a->sc_layouts = malloc(sizeof(VkImageLayout) * a->sc_image_count);
	vkGetSwapchainImagesKHR(a->device, a->swapchain,
			&a->sc_image_count, a->sc_images);

	for (uint32_t i = 0; i < a->sc_image_count; i++) {
		a->sc_layouts[i] = VK_IMAGE_LAYOUT_UNDEFINED;

		VkImageViewCreateInfo vci = {
			.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image    = a->sc_images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format   = a->sc_format,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.levelCount = 1,
				.layerCount = 1,
			},
		};
		VK_CHECK(vkCreateImageView(a->device, &vci, NULL, &a->sc_views[i]));
	}

	a->image_available = malloc(sizeof(VkSemaphore) * a->sc_image_count);
	a->render_finished = malloc(sizeof(VkSemaphore) * a->sc_image_count);
    for (uint32_t i = 0; i < a->sc_image_count; i++) {
        VkSemaphoreCreateInfo si = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VK_CHECK(vkCreateSemaphore(a->device, &si, NULL, &a->image_available[i]));
		VK_CHECK(vkCreateSemaphore(a->device, &si, NULL, &a->render_finished[i]));
    }
}

static void destroy_swapchain(t_app *a)
{
	for (uint32_t i = 0; i < a->sc_image_count; i++)
	{
		vkDestroySemaphore(a->device, a->image_available[i], NULL);
		vkDestroySemaphore(a->device, a->render_finished[i], NULL);
		vkDestroyImageView(a->device, a->sc_views[i], NULL);
	}
	free(a->sc_images);
	free(a->sc_views);
	free(a->sc_layouts);
	vkDestroySwapchainKHR(a->device, a->swapchain, NULL);
}

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

static void create_buffer(t_app *a, VkDeviceSize size, VkBufferUsageFlags usage,
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
        .memoryTypeIndex = find_memory_type(a, mr.memoryTypeBits,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
    };
    VK_CHECK(vkAllocateMemory(a->device, &mai, NULL, memory));
    VK_CHECK(vkBindBufferMemory(a->device, *buffer, *memory, 0));
}

static void upload_buffer(t_app *a, VkDeviceMemory memory,
                          const void *data, VkDeviceSize size)
{
    void *mapped;
    VK_CHECK(vkMapMemory(a->device, memory, 0, size, 0, &mapped));
    memcpy(mapped, data, size);
    vkUnmapMemory(a->device, memory);
}

static void create_scene_buffer(t_app *a)
{
    create_buffer(a, sizeof(t_scene_data),
                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  true, &a->scene_buffer, &a->scene_memory);

    VkBufferDeviceAddressInfo bdai = {
        .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = a->scene_buffer,
    };
    a->scene_address = vkGetBufferDeviceAddress(a->device, &bdai);
}

static void upload_mesh(t_app *a)
{
    Mesh *m = &a->mesh;

    /* vertex buffer */
    VkDeviceSize vert_size = sizeof(t_vertex) * m->vertex_count;
    create_buffer(a, vert_size,
                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  true, &a->vertex_buffer, &a->vertex_memory);
    upload_buffer(a, a->vertex_memory, m->vertices, vert_size);

    VkBufferDeviceAddressInfo bdai = {
        .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = a->vertex_buffer,
    };
    a->vertex_address = vkGetBufferDeviceAddress(a->device, &bdai);

    /* index buffer */
    VkDeviceSize idx_size = sizeof(uint32_t) * m->index_count;
    create_buffer(a, idx_size,
                  VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                  false, &a->index_buffer, &a->index_memory);
    upload_buffer(a, a->index_memory, m->indices, idx_size);

	t_scene_data scene_data = { a->vertex_address };
    upload_buffer(a, a->scene_memory, &scene_data, sizeof(t_scene_data));
}

/* ================================================================== */
/*  Bindless descriptors (Descriptor Indexing)
 *
 *  One descriptor set holds up to BINDLESS_TEXTURES combined-image-
 *  samplers.  Shaders index into it freely:
 *    layout(set=0, binding=0) uniform sampler2D textures[];
 *    vec4 c = texture(textures[push_const.tex_id], uv);
 * ================================================================== */
static void create_bindless_descriptors(t_app *a)
{
	/* Layout — variable-count binding with all the UPDATE_AFTER_BIND flags */
	VkDescriptorBindingFlags binding_flags =
		VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT     |
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT               |
		VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT             |
		VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;

	VkDescriptorSetLayoutBindingFlagsCreateInfo flags_ci = {
		.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
		.bindingCount  = 1,
		.pBindingFlags = &binding_flags,
	};

	VkDescriptorSetLayoutBinding layout_binding = {
		.binding         = 0,
		.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = BINDLESS_TEXTURES,
		.stageFlags      = VK_SHADER_STAGE_ALL,
	};

	VkDescriptorSetLayoutCreateInfo layout_ci = {
		.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext        = &flags_ci,
		.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
		.bindingCount = 1,
		.pBindings    = &layout_binding,
	};
	VK_CHECK(vkCreateDescriptorSetLayout(a->device, &layout_ci, NULL,
				&a->bindless_layout));

	/* Pool — must also carry UPDATE_AFTER_BIND flag */
	VkDescriptorPoolSize pool_size = {
		.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = BINDLESS_TEXTURES,
	};
	VkDescriptorPoolCreateInfo pool_ci = {
		.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
		.maxSets       = 1,
		.poolSizeCount = 1,
		.pPoolSizes    = &pool_size,
	};
	VK_CHECK(vkCreateDescriptorPool(a->device, &pool_ci, NULL,
				&a->bindless_pool));

	/* Allocate — specify the actual variable descriptor count */
	uint32_t var_count = BINDLESS_TEXTURES;
	VkDescriptorSetVariableDescriptorCountAllocateInfo var_ci = {
		.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
		.descriptorSetCount = 1,
		.pDescriptorCounts  = &var_count,
	};
	VkDescriptorSetAllocateInfo alloc_ci = {
		.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext              = &var_ci,
		.descriptorPool     = a->bindless_pool,
		.descriptorSetCount = 1,
		.pSetLayouts        = &a->bindless_layout,
	};
	VK_CHECK(vkAllocateDescriptorSets(a->device, &alloc_ci,
				&a->bindless_set));

	printf("Bindless descriptor set ready (%d slots)\n", BINDLESS_TEXTURES);
	/* To register a texture at index N:
	   VkDescriptorImageInfo img_info = { sampler, view, SHADER_READ_ONLY_OPTIMAL };
	   VkWriteDescriptorSet  write    = { ..., .dstBinding=0, .dstArrayElement=N,
	   .descriptorCount=1, .pImageInfo=&img_info };
	   vkUpdateDescriptorSets(device, 1, &write, 0, NULL);               */
}

/* ================================================================== */
/*  Command pool + per-frame sync                                     */
/* ================================================================== */
static void create_frame_resources(t_app *a)
{
	VkCommandPoolCreateInfo cp_ci = {
		.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = a->graphics_family,
	};
	VK_CHECK(vkCreateCommandPool(a->device, &cp_ci, NULL, &a->cmd_pool));

	VkCommandBuffer cmds[MAX_FRAMES];
	VkCommandBufferAllocateInfo ai = {
		.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool        = a->cmd_pool,
		.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = MAX_FRAMES,
	};
	VK_CHECK(vkAllocateCommandBuffers(a->device, &ai, cmds));

	for (int i = 0; i < MAX_FRAMES; i++) {
		a->frames[i].cmd = cmds[i];

		VkSemaphoreCreateInfo si = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		VK_CHECK(vkCreateSemaphore(a->device, &si, NULL, &a->frames[i].acquire_next_image));

		VkFenceCreateInfo fi = {
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT,
		};
		VK_CHECK(vkCreateFence(a->device, &fi, NULL,
					&a->frames[i].in_flight));
	}
}

/* ================================================================== */
/*  Synchronization2 helpers
 *
 *  We use VkImageMemoryBarrier2 which combines the old srcStageMask,
 *  dstStageMask, srcAccessMask, dstAccessMask into a single struct.
 *  No more "which stage do I put the wait mask in again?" confusion.
 * ================================================================== */
static void transition_image(t_app *a,
		VkCommandBuffer      cmd,
		VkImage              image,
		VkImageLayout        old_layout,
		VkImageLayout        new_layout,
		VkPipelineStageFlags2 src_stage,
		VkAccessFlags2        src_access,
		VkPipelineStageFlags2 dst_stage,
		VkAccessFlags2        dst_access)
{
	VkImageMemoryBarrier2 barrier = {
		.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask        = src_stage,
		.srcAccessMask       = src_access,
		.dstStageMask        = dst_stage,
		.dstAccessMask       = dst_access,
		.oldLayout           = old_layout,
		.newLayout           = new_layout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image               = image,
		.subresourceRange    = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.levelCount = 1,
			.layerCount = 1,
		},
	};

	VkDependencyInfo dep = {
		.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers    = &barrier,
	};

	/* Synchronization2: single call replaces vkCmdPipelineBarrier */
	a->fn_CmdPipelineBarrier2(cmd, &dep);
}

/* ================================================================== */
/*  Draw one frame
 *
 *  Dynamic Rendering: instead of VkRenderPassBeginInfo + framebuffers,
 *  we describe attachments inline with VkRenderingAttachmentInfo and
 *  call vkCmdBeginRendering / vkCmdEndRendering.
 * ================================================================== */
static int draw_frame(t_app *a)
{
	t_frame *f = &a->frames[a->frame_index % MAX_FRAMES];

	vkWaitForFences(a->device, 1, &f->in_flight, VK_TRUE, UINT64_MAX);

	uint32_t img_index;
	VkResult r = vkAcquireNextImageKHR(
			a->device, a->swapchain, UINT64_MAX,
			f->acquire_next_image, VK_NULL_HANDLE, &img_index);

	if (r == VK_ERROR_OUT_OF_DATE_KHR) return 0;
	if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) VK_CHECK(r);

	VkSemaphore tmp               = a->image_available[img_index];
	a->image_available[img_index] = f->acquire_next_image;
	f->acquire_next_image         = tmp;

	vkResetFences(a->device, 1, &f->in_flight);
	vkResetCommandBuffer(f->cmd, 0);

	/* ---- record --------------------------------------------------- */
	VkCommandBufferBeginInfo bi = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	VK_CHECK(vkBeginCommandBuffer(f->cmd, &bi));

	VkImage sc_img = a->sc_images[img_index];

	/* 1. Transition: UNDEFINED/PRESENT → COLOR_ATTACHMENT_OPTIMAL
	   (Synchronization2 barrier — cleaner than the old API)        */
	transition_image(a, f->cmd, sc_img,
			a->sc_layouts[img_index],
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,    0,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

	/* 2. Dynamic Rendering — describe the color attachment inline */
	VkClearValue clear = { .color = {{ 1.0f, 0.95f, 0.25f, 1.0f }} };

	VkRenderingAttachmentInfo color_att = {
		.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView   = a->sc_views[img_index],
		.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue  = clear,
	};

	VkRenderingInfo ri = {
		.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea           = { .extent = a->sc_extent },
		.layerCount           = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments    = &color_att,
		/* .pDepthAttachment / .pStencilAttachment = NULL — not needed here */
	};

	a->fn_CmdBeginRendering(f->cmd, &ri);

	VkViewport viewport = { 0, 0, (float)a->sc_extent.width, (float)a->sc_extent.height, 0, 1 };
	VkRect2D   scissor  = { {0,0}, a->sc_extent };

	vkCmdBindPipeline(f->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, a->pipeline);

	mat4 mvp;
	get_mvp(&a->camera, (float)a->sc_extent.width, (float)a->sc_extent.height, mvp);

	t_push_constants pc = {0};
	memcpy(pc.mvp, mvp, sizeof(mat4));
	pc.scene = a->scene_address;

	vkCmdPushConstants(f->cmd, a->pipeline_layout,
					   VK_SHADER_STAGE_VERTEX_BIT,
					   0, sizeof(t_push_constants), &pc);

	vkCmdSetViewport(f->cmd, 0, 1, &viewport);
	vkCmdSetScissor (f->cmd, 0, 1, &scissor);
	vkCmdBindIndexBuffer(f->cmd, a->index_buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(f->cmd, a->mesh.index_count, 1, 0, 0, 0);

	a->fn_CmdEndRendering(f->cmd);

	/* 3. Transition: COLOR_ATTACHMENT_OPTIMAL → PRESENT_SRC */
	transition_image(a, f->cmd, sc_img,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 0);

	a->sc_layouts[img_index] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VK_CHECK(vkEndCommandBuffer(f->cmd));

	/* ---- submit (Synchronization2: vkQueueSubmit2) ---------------- */
	VkSemaphoreSubmitInfo wait_si = {
		.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.semaphore = a->image_available[img_index],
		.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
	};
	VkCommandBufferSubmitInfo cmd_si = {
		.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
		.commandBuffer = f->cmd,
	};
	VkSemaphoreSubmitInfo signal_si = {
		.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.semaphore = a->render_finished[img_index],
		.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
	};
	VkSubmitInfo2 si2 = {
		.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.waitSemaphoreInfoCount   = 1,
		.pWaitSemaphoreInfos      = &wait_si,
		.commandBufferInfoCount   = 1,
		.pCommandBufferInfos      = &cmd_si,
		.signalSemaphoreInfoCount = 1,
		.pSignalSemaphoreInfos    = &signal_si,
	};
	VK_CHECK(a->fn_QueueSubmit2(a->graphics_q, 1, &si2, f->in_flight));

	/* ---- present -------------------------------------------------- */
	VkPresentInfoKHR pi = {
		.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores    = &a->render_finished[img_index],
		.swapchainCount     = 1,
		.pSwapchains        = &a->swapchain,
		.pImageIndices      = &img_index,
	};
	r = vkQueuePresentKHR(a->present_q, &pi);
	if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) return 0;
	VK_CHECK(r);

	a->frame_index++;
	return 1;
}

static void create_graphics_pipeline(t_app *a)
{
	/* load spirv */
    size_t   vert_size, frag_size;
    uint32_t *vert_code = load_spirv_file("assets/shaders/triangle.vert.spv", &vert_size);
    uint32_t *frag_code = load_spirv_file("assets/shaders/triangle.frag.spv", &frag_size);

    VkShaderModule vert_module, frag_module;

    VkShaderModuleCreateInfo vert_info = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vert_size,
        .pCode    = vert_code,
    };
    VkShaderModuleCreateInfo frag_info = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = frag_size,
        .pCode    = frag_code,
    };
    VK_CHECK(vkCreateShaderModule(a->device, &vert_info, NULL, &vert_module));
    VK_CHECK(vkCreateShaderModule(a->device, &frag_info, NULL, &frag_module));
    free(vert_code);
    free(frag_code);

	VkPipelineShaderStageCreateInfo stages[2] = {
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vert_module,
            .pName  = "main",
        },
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = frag_module,
            .pName  = "main",
        },
    };

	// vertex input
	VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 0,
        .vertexAttributeDescriptionCount = 0,
    };

	// shape (triangle)
	VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

	VkPipelineViewportStateCreateInfo viewport_state = {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount  = 1,
    };

	// rasterizer
	VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
#ifndef WIREFRAME
        .polygonMode = VK_POLYGON_MODE_FILL,
# else
        .polygonMode = VK_POLYGON_MODE_LINE,
#endif
        .cullMode    = VK_CULL_MODE_NONE,   /* no backface culling for now */
        .frontFace   = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth   = 1.0f,
    };

	VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,	/* no multi sampling */
    };

    VkPipelineColorBlendAttachmentState blend_attachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable    = VK_FALSE,
    };
    VkPipelineColorBlendStateCreateInfo color_blending = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &blend_attachment,
    };

	VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates    = dynamic_states,
    };

	// pipeline layout
	VkPushConstantRange pc_range = {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.offset     = 0,
    	.size       = sizeof(t_push_constants),
	};

    VkPipelineLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pushConstantRangeCount = 1,
    	.pPushConstantRanges    = &pc_range,
    };
    VK_CHECK(vkCreatePipelineLayout(a->device, &layout_ci, NULL, &a->pipeline_layout));

	VkPipelineRenderingCreateInfo rendering_ci = {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount    = 1,
        .pColorAttachmentFormats = &a->sc_format,
    };

	// assemble the pipeline
	VkGraphicsPipelineCreateInfo pipeline_ci = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext               = &rendering_ci,   /* dynamic rendering hook */
        .stageCount          = 2,
        .pStages             = stages,
        .pVertexInputState   = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState      = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState   = &multisampling,
        .pColorBlendState    = &color_blending,
        .pDynamicState       = &dynamic_state,
        .layout              = a->pipeline_layout,
        .renderPass          = VK_NULL_HANDLE,  /* not needed with dynamic rendering */
    };

	VK_CHECK(vkCreateGraphicsPipelines(a->device, VK_NULL_HANDLE, 1, &pipeline_ci, NULL, &a->pipeline));

	vkDestroyShaderModule(a->device, vert_module, NULL);
    vkDestroyShaderModule(a->device, frag_module, NULL);
}

/* ================================================================== */
/*  Main                                                              */
/* ================================================================== */
int main(void)
{
	t_app a = {0};

	if (!SDL_Init(SDL_INIT_VIDEO)) {
		fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		return 1;
	}

	a.window = SDL_CreateWindow(APP_NAME, WIN_W, WIN_H,
			SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	if (!a.window) {
		fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
		return 1;
	}
	SDL_SetWindowRelativeMouseMode(a.window, true);

	create_instance(&a);

	if (!SDL_Vulkan_CreateSurface(a.window, a.instance, NULL, &a.surface)) {
		fprintf(stderr, "SDL_Vulkan_CreateSurface: %s\n", SDL_GetError());
		return 1;
	}

	pick_physical_device(&a);
	create_device(&a);
	create_swapchain(&a);
	create_scene_buffer(&a);
	a.mesh = load_mesh_from_gltf_file("assets/models/cube.glb");
	upload_mesh(&a);
	create_bindless_descriptors(&a);
	create_graphics_pipeline(&a);
	create_frame_resources(&a);

	init_camera(&a.camera);
	uint64_t last_time = SDL_GetTicksNS();
	float dt = 1.0f / 60.0f;

	/* main loop */
	bool running = true;
	while (running) {
		uint64_t cur_time = SDL_GetTicksNS();
		dt = (float)(cur_time - last_time) / 1000000000.0f;
		last_time = cur_time;
		//printf("FPS: %d\n", (int)(1.0f / delta_time));

		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_EVENT_QUIT || (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE))
				running = false;
			if (e.type == SDL_EVENT_WINDOW_RESIZED ||
					e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
				vkDeviceWaitIdle(a.device);
				destroy_swapchain(&a);
				create_swapchain(&a);
			}
			if (e.type == SDL_EVENT_MOUSE_MOTION)
				camera_rotate(&a.camera, e.motion.xrel * dt, e.motion.yrel * dt);
		}

		const bool *keyboard_state = SDL_GetKeyboardState(NULL);
		camera_move(&a.camera, keyboard_state, dt);

		SDL_WindowFlags wf = SDL_GetWindowFlags(a.window);
		if (wf & SDL_WINDOW_MINIMIZED) { SDL_Delay(16); continue; }

		if (!draw_frame(&a)) {
			vkDeviceWaitIdle(a.device);
			destroy_swapchain(&a);
			create_swapchain(&a);
		}
	}

	/* cleanup */
	vkDeviceWaitIdle(a.device);

	vkDestroyPipeline      (a.device, a.pipeline,        NULL);
	vkDestroyPipelineLayout(a.device, a.pipeline_layout, NULL);

	for (int i = 0; i < MAX_FRAMES; i++)
	{
		vkDestroyFence (a.device, a.frames[i].in_flight, NULL);
		vkDestroySemaphore(a.device, a.frames[i].acquire_next_image, NULL);
	}

	vkDestroyCommandPool(a.device, a.cmd_pool, NULL);

	vkDestroyDescriptorPool      (a.device, a.bindless_pool,   NULL);
	vkDestroyDescriptorSetLayout (a.device, a.bindless_layout, NULL);

	vkFreeMemory   (a.device, a.index_memory, NULL);
	vkDestroyBuffer(a.device, a.index_buffer, NULL);
	vkFreeMemory  (a.device, a.vertex_memory, NULL);
	vkDestroyBuffer(a.device, a.vertex_buffer, NULL);
	vkFreeMemory  (a.device, a.scene_memory, NULL);
	vkDestroyBuffer(a.device, a.scene_buffer, NULL);

	destroy_swapchain(&a);
	vkDestroyDevice    (a.device,   NULL);
	vkDestroySurfaceKHR(a.instance, a.surface, NULL);
	vkDestroyInstance  (a.instance, NULL);

	SDL_DestroyWindow(a.window);
	SDL_Quit();
	return 0;
}
