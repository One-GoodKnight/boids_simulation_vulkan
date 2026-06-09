#include "vulkan/swapchain.h"

/* ================================================================== */
/*  Swapchain (no render pass / framebuffers needed with dynrender)   */
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

void create_swapchain(t_app *a)
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

void destroy_swapchain(t_app *a)
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
