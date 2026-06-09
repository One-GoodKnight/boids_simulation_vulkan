#ifndef APP_H
# define APP_H

# include <SDL3/SDL.h>
# include <vulkan/vulkan.h>
# include <SDL3/SDL_vulkan.h>

# include <cglm/cglm.h>

#include "camera.h"
# include "geometry.h"

/* ------------------------------------------------------------------- */
/*  Config                                                             */
/* ------------------------------------------------------------------- */
#define APP_NAME            "Vulkan app"
#define WIN_W               1200
#define WIN_H               1200
#define MAX_FRAMES          2
#define BINDLESS_TEXTURES   1024

/* ------------------------------------------------------------------ */
/*  Per-frame resources                                               */
/* ------------------------------------------------------------------ */
typedef struct s_frame {
	VkCommandBuffer cmd;
	VkSemaphore 	acquire_next_image; /* handed to vkAcquireNextImageKHR      */
	VkFence         in_flight;          /* CPU waits on this                    */
} t_frame;

/* ------------------------------------------------------------------ */
/*  Application state                                                 */
/* ------------------------------------------------------------------ */
typedef struct s_app {
	SDL_Window       *window;

	/* core Vulkan objects */
	VkInstance        instance;
	VkSurfaceKHR      surface;
	VkPhysicalDevice  physical;
	VkDevice          device;
	uint32_t          graphics_family;
	uint32_t          present_family;
	VkQueue           graphics_q;
	VkQueue           present_q;

	/* function pointers for Vulkan 1.3 entry points
	   (loaded dynamically so we work even if the header predates 1.3) */
	PFN_vkCmdBeginRendering    fn_CmdBeginRendering;
	PFN_vkCmdEndRendering      fn_CmdEndRendering;
	PFN_vkCmdPipelineBarrier2  fn_CmdPipelineBarrier2;
	PFN_vkQueueSubmit2         fn_QueueSubmit2;

	/* swapchain */
	VkSwapchainKHR    swapchain;
	VkFormat          sc_format;
	VkExtent2D        sc_extent;
	uint32_t          sc_image_count;
	VkImage          *sc_images;
	VkImageView      *sc_views;
	/* track layout per image for barrier reuse */
	VkImageLayout    *sc_layouts;
	VkSemaphore      *image_available;
	VkSemaphore      *render_finished;

	/* depth buffer */
	VkImage        depth_image;
	VkDeviceMemory depth_memory;
	VkImageView    depth_view;
	VkFormat       depth_format;

	/* commands */
	VkCommandPool     cmd_pool;
	t_frame           frames[MAX_FRAMES];
	uint32_t          frame_index;

	/* Buffer Device Address */
	VkBuffer          vertex_buffer;
	VkDeviceMemory    vertex_memory;

	VkBuffer          index_buffer;
	VkDeviceMemory    index_memory;

	VkBuffer          boid_buffer;
	VkDeviceMemory    boid_memory;
	uint32_t          boid_count;

	VkBuffer          scene_buffer;
	VkDeviceMemory    scene_memory;
	VkDeviceAddress   scene_address;

	Mesh              mesh;

	/* Bindless (Descriptor Indexing) */
	VkDescriptorSetLayout bindless_layout;
	VkDescriptorPool      bindless_pool;
	VkDescriptorSet       bindless_set;

	/* Pipelines */
	VkPipelineLayout  pipeline_graphics_layout;
	VkPipeline        pipeline_graphics;
	VkPipelineLayout  pipeline_compute_layout;
	VkPipeline        pipeline_compute;

	t_camera          camera;
} t_app;

/* ------------------------------------------------------------------- */
/*  Error helper                                                       */
/* ------------------------------------------------------------------- */
# define VK_CHECK(x)                                                    \
	do {                                                                \
		VkResult _r = (x);                                            	\
		if (_r != VK_SUCCESS) {                                       	\
			fprintf(stderr, "Vulkan error %d at %s:%d\n",            	\
					_r, __FILE__, __LINE__);                          	\
			exit(1);                                                  	\
		}                                                            	\
	} while (0)

#endif
