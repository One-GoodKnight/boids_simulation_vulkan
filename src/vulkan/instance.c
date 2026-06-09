#include "vulkan/instance.h"

/* ================================================================== */
/*  Instance  (Vulkan 1.3 requested)                                  */
/* ================================================================== */
void create_instance(t_app *a)
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

