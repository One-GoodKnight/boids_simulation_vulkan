/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   app.h                                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aginiaux <aginiaux@student.42lyon.fr>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/09 13:27:46 by aginiaux          #+#    #+#             */
/*   Updated: 2026/06/11 20:23:46 by aginiaux         ###   ########lyon.fr   */
/*                                                                            */
/* ************************************************************************** */

#ifndef APP_H
# define APP_H

# include <SDL3/SDL.h>
# include <vulkan/vulkan.h>
# include <SDL3/SDL_vulkan.h>

# include <cglm/cglm.h>

# include "camera.h"
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

	/* function pointers for Vulkan 1.3 entry points, might want to look into Volt */
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

	VkBuffer          debug_buffer;
	VkDeviceMemory    debug_memory;
	VkDeviceAddress   debug_address;

	Mesh              mesh;

	/* Pipelines */
	VkPipelineLayout  pipeline_compute_layout;
	VkPipeline        pipeline_compute;

	VkPipeline        pipeline_graphics;
	VkPipelineLayout  pipeline_graphics_layout;
	VkPipeline        pipeline_outline;
	VkPipelineLayout  pipeline_outline_layout;

	/* Spatial hash grid */
	VkPipelineLayout  pipeline_compute_spatial_hash_grid_layout;

	VkBuffer          boid_slot_buffer;   /* which slot each boid belongs to */
    VkDeviceMemory    boid_slot_memory;
    VkDeviceAddress   boid_slot_address;

	VkBuffer          slot_boid_count_buffer;   /* how many boids per slot */
    VkDeviceMemory    slot_boid_count_memory;
    VkDeviceAddress   slot_boid_count_address;

	VkBuffer          slot_offset_buffer;   /* at what offset in the sorted boids buffer the slot starts */
    VkDeviceMemory    slot_offset_memory;
    VkDeviceAddress   slot_offset_address;

	VkBuffer          slot_cursor_buffer;   /* copy of slot_offset that we will modify during the creation of sorted_boid */
	VkDeviceMemory    slot_cursor_memory;
	VkDeviceAddress   slot_cursor_address;

	VkBuffer          sorted_boid_buffer;   /* boids sorted by slot */
	VkDeviceMemory    sorted_boid_memory;
	VkDeviceAddress   sorted_boid_address;

	uint32_t          slot_count;
	uint32_t          slot_count_padded;

	VkPipeline        pipeline_compute_boid_slot;
	VkPipeline        pipeline_compute_slot_boid_count;
	VkPipeline        pipeline_compute_slot_offset_upsweep;
	VkPipeline        pipeline_compute_slot_offset_downsweep;
	VkPipeline        pipeline_compute_sorted_boid;

	/* Bindless (Descriptor Indexing) */
	VkDescriptorSetLayout bindless_layout;
	VkDescriptorPool      bindless_pool;
	VkDescriptorSet       bindless_set;

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
