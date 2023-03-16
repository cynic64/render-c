#ifndef LL_SET_H
#define LL_SET_H

#include <vulkan/vulkan.h>

#include <assert.h>
#include <stdlib.h>
#include <vulkan/vulkan_core.h>
#include <string.h>

struct DescriptorInfo {
        VkDescriptorType type;
        VkShaderStageFlags shader_stage_flags;
        // Only for buffers
        VkDescriptorBufferInfo buffer;
        // Only for images
        VkDescriptorImageInfo image;
};

struct SetInfo {
	int desc_ct;
	struct DescriptorInfo* descs;
};

// Only supports one set of just uniform buffers for now.
void dpool_create(VkDevice device, int desc_ct, struct DescriptorInfo* descs, VkDescriptorPool *dpool) {
	for (int i = 0; i < desc_ct; i++) {
		assert(descs[i].type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	}

	VkDescriptorPoolSize dpool_size = {0};
	dpool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	dpool_size.descriptorCount = desc_ct;

	VkDescriptorPoolCreateInfo dpool_info = {0};
	dpool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	dpool_info.maxSets = 1;
	dpool_info.poolSizeCount = 1;
	dpool_info.pPoolSizes = &dpool_size;

	VkResult res = vkCreateDescriptorPool(device, &dpool_info, NULL, dpool);
	assert(res == VK_SUCCESS);
}

// Only needs `type` and `shader_stage_flags` from each DescriptorInfo. Useful because one set works
// for many buffers.
void set_layout_create(VkDevice device, struct SetInfo* set_info, VkDescriptorSetLayout* layout)
{
	VkDescriptorSetLayoutBinding* bindings = malloc(sizeof(bindings[0]) * set_info->desc_ct);
	bzero(bindings, sizeof(bindings[0]) * set_info->desc_ct);
	for (int i = 0; i < set_info->desc_ct; i++) {
		bindings[i].binding = i;
		// This is only ever >1 for things accessed in the shader as arrays. I don't use
		// those for now, so don't bother.
		bindings[i].descriptorCount = 1;
		bindings[i].descriptorType = set_info->descs[i].type;
		bindings[i].stageFlags = set_info->descs[i].shader_stage_flags;
	}

        VkDescriptorSetLayoutCreateInfo info = {0};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = set_info->desc_ct;
        info.pBindings = bindings;

        VkResult res = vkCreateDescriptorSetLayout(device, &info, NULL, layout);
        assert(res == VK_SUCCESS);

	free(bindings);
}

void set_create(VkDevice device, VkDescriptorPool dpool, VkDescriptorSetLayout layout,
		struct SetInfo* set_info, VkDescriptorSet *set)
{
        VkDescriptorSetAllocateInfo info = {0};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        info.descriptorPool = dpool;
        info.descriptorSetCount = 1;
        info.pSetLayouts = &layout;

        VkResult res = vkAllocateDescriptorSets(device, &info, set);
        assert(res == VK_SUCCESS);

        VkWriteDescriptorSet* writes = malloc(set_info->desc_ct * sizeof(writes[0]));
	bzero(writes, set_info->desc_ct * sizeof(writes[0]));
        for (int i = 0; i < set_info->desc_ct; ++i) {
                writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[i].dstSet = *set;
                writes[i].dstBinding = i;
                writes[i].dstArrayElement = 0;
                writes[i].descriptorType = set_info->descs[i].type;
                writes[i].descriptorCount = 1;

                if (set_info->descs[i].type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
                        writes[i].pBufferInfo = &set_info->descs[i].buffer;
                } else if (set_info->descs[i].type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
                        writes[i].pImageInfo = &set_info->descs[i].image;
                }
        }

        vkUpdateDescriptorSets(device, set_info->desc_ct, writes, 0, NULL);

        free(writes);
}

#endif // LL_SET_H
