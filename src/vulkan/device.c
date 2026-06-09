#include "vulkan/device.h"

/* ================================================================== */
/*  Physical device selection                                          */
/* ================================================================== */
void pick_physical_device(t_app *a)
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
void create_device(t_app *a)
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
