#include "vulkan/bindless.h"

/* ================================================================== */
/*  Bindless descriptors (Descriptor Indexing)
 *
 *  One descriptor set holds up to BINDLESS_TEXTURES combined-image-
 *  samplers.  Shaders index into it freely:
 *    layout(set=0, binding=0) uniform sampler2D textures[];
 *    vec4 c = texture(textures[push_const.tex_id], uv);
 * ================================================================== */
void create_bindless_descriptors(t_app *a)
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

