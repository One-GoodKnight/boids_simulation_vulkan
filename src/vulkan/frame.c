#include "vulkan/frame.h"

/* ================================================================== */
/*  Command pool + per-frame sync                                     */
/* ================================================================== */
void create_frame_resources(t_app *a)
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
