#include <SDL3/SDL_events.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "boids.h"
#include "vulkan/BDA.h"
#include "vulkan/depth.h"
#include "vulkan/pipeline.h"
#include "vulkan/swapchain.h"
#include "vulkan/shader_types.h"
#include "app.h"
#include "camera.h"
#include "load_files.h"

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
		VkAccessFlags2        dst_access,
		VkImageAspectFlags    aspect)
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
			.aspectMask = aspect,
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
static int draw_frame(t_app *a, float dt)
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

	/* Record command buffer */
	VkCommandBufferBeginInfo bi = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	VK_CHECK(vkBeginCommandBuffer(f->cmd, &bi));

	VkImage sc_img = a->sc_images[img_index];

	/* Compute - update boids */
	vkCmdBindPipeline(f->cmd, VK_PIPELINE_BIND_POINT_COMPUTE, a->pipeline_compute);

	t_push_constants_compute cpc = {
		.scene = a->scene_address,
		.dt = dt,
		.boid_count = a->boid_count,
		.max_dist = MAX_DISTANCE,
		.min_vel = BOID_MIN_VEL,
		.max_vel = BOID_MAX_VEL,
		.separation_radius = BOID_SEPARATION_RADIUS,
		.separation_force = BOID_SEPARATION_FORCE,
		.alignment_radius = BOID_ALIGNMENT_RADIUS,
		.alignment_force = BOID_ALIGNMENT_FORCE,
		.cohesion_radius = BOID_COHESION_RADIUS,
		.cohesion_force = BOID_COHESION_FORCE,
		.avoid_border_force = BOID_AVOID_BORDER_FORCE,
	};
	vkCmdPushConstants(f->cmd, a->pipeline_compute_layout,
					   VK_SHADER_STAGE_COMPUTE_BIT,
					   0, sizeof(t_push_constants_compute), &cpc);
	vkCmdDispatch(f->cmd, (a->boid_count + 63) / 64, 1, 1);

	/* Barrier — compute write must finish before vertex shader reads */
	VkBufferMemoryBarrier2 boid_barrier = {
		.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
		.srcStageMask        = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		.srcAccessMask       = VK_ACCESS_2_SHADER_WRITE_BIT,
		.dstStageMask        = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
		.dstAccessMask       = VK_ACCESS_2_SHADER_READ_BIT,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.buffer              = a->boid_buffer,
		.size                = VK_WHOLE_SIZE,
	};
	VkDependencyInfo dep = {
		.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.bufferMemoryBarrierCount = 1,
		.pBufferMemoryBarriers    = &boid_barrier,
	};
	a->fn_CmdPipelineBarrier2(f->cmd, &dep);

	/* Transition: UNDEFINED/PRESENT → COLOR_ATTACHMENT_OPTIMAL */
	transition_image(a, f->cmd, sc_img,
			a->sc_layouts[img_index],
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT);

	/* Transition: UNDEFINED → DEPTH_ATTACHMENT_OPTIMAL */
	transition_image(a, f->cmd, a->depth_image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0,
			VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
			VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_ASPECT_DEPTH_BIT);

	/* Clear color */
	VkClearValue clear = { .color = {{ 1.0f, 0.95f, 0.25f, 1.0f }} };

	VkRenderingAttachmentInfo color_att = {
		.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView   = a->sc_views[img_index],
		.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue  = clear,
	};

	VkRenderingAttachmentInfo depth_att = {
		.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView   = a->depth_view,
		.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
		.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp     = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.clearValue  = { .depthStencil = { 1.0f, 0 } },
	};

	VkRenderingInfo ri = {
		.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea           = { .extent = a->sc_extent },
		.layerCount           = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments    = &color_att,
		.pDepthAttachment     = &depth_att,
	};

	a->fn_CmdBeginRendering(f->cmd, &ri);

	VkViewport viewport = { 0, 0, (float)a->sc_extent.width, (float)a->sc_extent.height, 0, 1 };
	VkRect2D   scissor  = { {0,0}, a->sc_extent };

	vkCmdBindPipeline(f->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, a->pipeline_graphics);

	mat4 mvp;
	get_mvp(&a->camera, (float)a->sc_extent.width, (float)a->sc_extent.height, mvp);

	t_push_constants_graphics gpc = {
		.scene = a->scene_address,
		.min_vel = BOID_MIN_VEL,
		.max_vel = BOID_MAX_VEL,
	};
	memcpy(gpc.mvp, mvp, sizeof(mat4));

	vkCmdPushConstants(f->cmd, a->pipeline_graphics_layout,
					   VK_SHADER_STAGE_VERTEX_BIT,
					   0, sizeof(t_push_constants_graphics), &gpc);

	vkCmdSetViewport(f->cmd, 0, 1, &viewport);
	vkCmdSetScissor (f->cmd, 0, 1, &scissor);
	vkCmdBindIndexBuffer(f->cmd, a->index_buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(f->cmd, a->mesh.index_count, a->boid_count, 0, 0, 0);

	a->fn_CmdEndRendering(f->cmd);

	/* Transition: COLOR_ATTACHMENT_OPTIMAL → PRESENT_SRC */
	transition_image(a, f->cmd, sc_img,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 0,
			VK_IMAGE_ASPECT_COLOR_BIT);

	a->sc_layouts[img_index] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VK_CHECK(vkEndCommandBuffer(f->cmd));

	/* Submit command buffer */
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
	create_depth_buffer(&a);
	create_scene_buffer(&a);
	a.mesh = load_mesh_from_gltf_file("assets/models/cone.glb");
	upload_mesh(&a);
	upload_boids(&a, 30000);
	upload_scene(&a);
	create_bindless_descriptors(&a);
	create_graphics_pipeline(&a);
	create_compute_pipeline(&a);
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
		// printf("FPS: %d\n", (int)(1.0f / dt));

		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_EVENT_QUIT || (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE))
				running = false;
			if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_LALT)
			{
				SDL_WarpMouseInWindow(a.window, (float)a.sc_extent.width / 2, (float)a.sc_extent.height / 2);
				SDL_SetWindowRelativeMouseMode(a.window, false);
			}
			if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT)
				SDL_SetWindowRelativeMouseMode(a.window, true);
			if (e.type == SDL_EVENT_WINDOW_RESIZED ||
					e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
				vkDeviceWaitIdle(a.device);
				destroy_swapchain(&a);
				create_swapchain(&a);
				destroy_depth_resources(&a);
				create_depth_buffer(&a);
			}
			if (e.type == SDL_EVENT_MOUSE_MOTION)
				camera_rotate(&a.camera, e.motion.xrel, e.motion.yrel);
		}

		const bool *keyboard_state = SDL_GetKeyboardState(NULL);
		camera_move(&a.camera, keyboard_state, dt);

		SDL_WindowFlags wf = SDL_GetWindowFlags(a.window);
		if (wf & SDL_WINDOW_MINIMIZED) { SDL_Delay(16); continue; }

		if (!draw_frame(&a, dt)) {
			vkDeviceWaitIdle(a.device);
			destroy_swapchain(&a);
			create_swapchain(&a);
			destroy_depth_resources(&a);
			create_depth_buffer(&a);
		}
	}

	/* cleanup */
	vkDeviceWaitIdle(a.device);

	vkDestroyPipeline      (a.device, a.pipeline_compute,        NULL);
	vkDestroyPipelineLayout(a.device, a.pipeline_compute_layout, NULL);
	vkDestroyPipeline      (a.device, a.pipeline_graphics,        NULL);
	vkDestroyPipelineLayout(a.device, a.pipeline_graphics_layout, NULL);

	for (int i = 0; i < MAX_FRAMES; i++)
	{
		vkDestroyFence (a.device, a.frames[i].in_flight, NULL);
		vkDestroySemaphore(a.device, a.frames[i].acquire_next_image, NULL);
	}

	vkDestroyCommandPool(a.device, a.cmd_pool, NULL);

	vkDestroyDescriptorPool      (a.device, a.bindless_pool,   NULL);
	vkDestroyDescriptorSetLayout (a.device, a.bindless_layout, NULL);

	vkFreeMemory   (a.device, a.boid_memory, NULL);
	vkDestroyBuffer(a.device, a.boid_buffer, NULL);
	vkFreeMemory   (a.device, a.index_memory, NULL);
	vkDestroyBuffer(a.device, a.index_buffer, NULL);
	vkFreeMemory  (a.device, a.vertex_memory, NULL);
	vkDestroyBuffer(a.device, a.vertex_buffer, NULL);
	vkFreeMemory  (a.device, a.scene_memory, NULL);
	vkDestroyBuffer(a.device, a.scene_buffer, NULL);

	destroy_depth_resources(&a);
	destroy_swapchain(&a);
	vkDestroyDevice    (a.device,   NULL);
	vkDestroySurfaceKHR(a.instance, a.surface, NULL);
	vkDestroyInstance  (a.instance, NULL);

	SDL_DestroyWindow(a.window);
	SDL_Quit();
	return 0;
}
