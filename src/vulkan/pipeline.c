/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   pipeline.c                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aginiaux <aginiaux@student.42lyon.fr>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/09 13:20:27 by aginiaux          #+#    #+#             */
/*   Updated: 2026/06/11 12:46:10 by aginiaux         ###   ########lyon.fr   */
/*                                                                            */
/* ************************************************************************** */

#include <vulkan/vulkan_core.h>

#include "vulkan/pipeline.h"
#include "load_files.h"
#include "shader_types.h"

static VkShaderModule create_shader_module(t_app *a, const char *path)
{
    size_t   size;
    uint32_t *code = load_spirv_file(path, &size);

    VkShaderModuleCreateInfo ci = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode    = code,
    };
    VkShaderModule module;
    VK_CHECK(vkCreateShaderModule(a->device, &ci, NULL, &module));
    free(code);
    return module;
}

static VkPipelineLayout create_pipeline_layout(
    VkDevice               device,
    uint32_t               set_count,
    VkDescriptorSetLayout *set_layouts,
    uint32_t               pc_count,
    VkPushConstantRange   *pc_ranges)
{
    VkPipelineLayoutCreateInfo ci = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = set_count,
        .pSetLayouts            = set_layouts,
        .pushConstantRangeCount = pc_count,
        .pPushConstantRanges    = pc_ranges,
    };
    VkPipelineLayout layout;
    VK_CHECK(vkCreatePipelineLayout(device, &ci, NULL, &layout));
    return layout;
}

void create_compute_spatial_hash_pipelines(
	t_app *a,
	const char *boid_slot_path,
	const char *slot_boid_count_path,
	const char *slot_offset_upsweep
)
{
	VkPushConstantRange pc_range = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset     = 0,
        .size       = sizeof(t_push_constants_compute),
    };
    a->pipeline_compute_spatial_hash_grid_layout = create_pipeline_layout(a->device, 0, NULL, 1, &pc_range);

    VkShaderModule boid_slot_module = create_shader_module(a, boid_slot_path);
	VkComputePipelineCreateInfo ci = {
        .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage  = {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = boid_slot_module,
            .pName  = "main",
        },
        .layout = a->pipeline_compute_spatial_hash_grid_layout,
    };
    VK_CHECK(vkCreateComputePipelines(a->device, VK_NULL_HANDLE, 1, &ci, NULL, &a->pipeline_compute_boid_slot));
    vkDestroyShaderModule(a->device, boid_slot_module, NULL);

	VkShaderModule slot_boid_count_module = create_shader_module(a, slot_boid_count_path);
	ci.stage.module = slot_boid_count_module;
    VK_CHECK(vkCreateComputePipelines(a->device, VK_NULL_HANDLE, 1, &ci, NULL, &a->pipeline_compute_slot_boid_count));
    vkDestroyShaderModule(a->device, slot_boid_count_module, NULL);

	VkShaderModule slot_offset_upsweep_module = create_shader_module(a, slot_offset_upsweep);
	ci.stage.module = slot_offset_upsweep_module;
    VK_CHECK(vkCreateComputePipelines(a->device, VK_NULL_HANDLE, 1, &ci, NULL, &a->pipeline_compute_slot_offset_upsweep));
    vkDestroyShaderModule(a->device, slot_offset_upsweep_module, NULL);
}

void create_compute_pipeline(t_app *a, const char *path)
{
    VkShaderModule comp_module = create_shader_module(a, path);

    VkPushConstantRange pc_range = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset     = 0,
        .size       = sizeof(t_push_constants_compute),
    };
    a->pipeline_compute_layout = create_pipeline_layout(a->device, 0, NULL, 1, &pc_range);

    VkComputePipelineCreateInfo ci = {
        .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage  = {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = comp_module,
            .pName  = "main",
        },
        .layout = a->pipeline_compute_layout,
    };
    VK_CHECK(vkCreateComputePipelines(a->device, VK_NULL_HANDLE, 1, &ci, NULL, &a->pipeline_compute));

    vkDestroyShaderModule(a->device, comp_module, NULL);
}

void create_graphics_pipeline(t_app *a)
{
    VkShaderModule vert_module = create_shader_module(a, "assets/shaders/boids_graphics.vert.spv");
	VkShaderModule frag_module = create_shader_module(a, "assets/shaders/boids_graphics.frag.spv");

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
        .cullMode    = VK_CULL_MODE_BACK_BIT,   /* no backface culling for now */
        .frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE,
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

	VkPipelineDepthStencilStateCreateInfo depth_stencil = {
		.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable       = VK_TRUE,
		.depthWriteEnable      = VK_TRUE,
		.depthCompareOp        = VK_COMPARE_OP_LESS,
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable     = VK_FALSE,
	};

	// pipeline layout
	VkPushConstantRange pc_range = {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		.offset     = 0,
		.size       = sizeof(t_push_constants_graphics),
	};
	a->pipeline_graphics_layout = create_pipeline_layout(a->device, 0, NULL, 1, &pc_range);

	VkPipelineRenderingCreateInfo rendering_ci = {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount    = 1,
        .pColorAttachmentFormats = &a->sc_format,
		.depthAttachmentFormat   = a->depth_format,
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
		.pDepthStencilState  = &depth_stencil,
        .layout              = a->pipeline_graphics_layout,
        .renderPass          = VK_NULL_HANDLE,  /* not needed with dynamic rendering */
    };

	VK_CHECK(vkCreateGraphicsPipelines(a->device, VK_NULL_HANDLE, 1, &pipeline_ci, NULL, &a->pipeline_graphics));

	vkDestroyShaderModule(a->device, vert_module, NULL);
    vkDestroyShaderModule(a->device, frag_module, NULL);
}

void create_outline_pipeline(t_app *a)
{
    VkShaderModule vert_module = create_shader_module(a, "assets/shaders/boids_graphics.vert.spv");
    VkShaderModule frag_module = create_shader_module(a, "assets/shaders/boids_graphics.frag.spv");

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

    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount  = 1,
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode    = VK_CULL_MODE_FRONT_BIT,
        .frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth   = 1.0f,
		.depthBiasEnable  = VK_TRUE,
		.depthBiasConstantFactor = 0.0f,
		.depthBiasSlopeFactor    = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
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

    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable   = VK_TRUE,
        .depthWriteEnable  = VK_FALSE,
		.depthCompareOp    = VK_COMPARE_OP_LESS,
    };

    VkPushConstantRange pc_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset     = 0,
        .size       = sizeof(t_push_constants_graphics),
    };
    a->pipeline_outline_layout = create_pipeline_layout(a->device, 0, NULL, 1, &pc_range);

    VkPipelineRenderingCreateInfo rendering_ci = {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount    = 1,
        .pColorAttachmentFormats = &a->sc_format,
        .depthAttachmentFormat   = a->depth_format,
    };

    VkGraphicsPipelineCreateInfo pipeline_ci = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext               = &rendering_ci,
        .stageCount          = 2,
        .pStages             = stages,
        .pVertexInputState   = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState      = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState   = &multisampling,
        .pColorBlendState    = &color_blending,
        .pDynamicState       = &dynamic_state,
        .pDepthStencilState  = &depth_stencil,
        .layout              = a->pipeline_outline_layout,
        .renderPass          = VK_NULL_HANDLE,
    };

    VK_CHECK(vkCreateGraphicsPipelines(a->device, VK_NULL_HANDLE, 1, &pipeline_ci, NULL, &a->pipeline_outline));

    vkDestroyShaderModule(a->device, vert_module, NULL);
    vkDestroyShaderModule(a->device, frag_module, NULL);
}
