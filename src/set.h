#ifndef LL_SET_H
#define LL_SET_H

#include <vulkan/vulkan.h>

#include <assert.h>
#include <stdlib.h>
#include <vulkan/vulkan_core.h>
#include <string.h>

struct DescriptorInfo {
        VkDescriptorType type;
        VkShaderStageFlags stage;
};

struct SetInfo {
	int desc_ct;
	struct DescriptorInfo* descs;
};

// Sets can use buffers or images, so we need to be able to pass either one to `set_create`
union SetHandle {
	VkDescriptorBufferInfo buffer;
	VkDescriptorImageInfo image;
};

// Don't use this if you have complicated requirements, just do it yourself. If you have different
// sets for every frame, this function isn't smart enough to work.
void dpool_create(VkDevice device, int set_ct, int desc_ct,
		  struct DescriptorInfo* descs, VkDescriptorPool *dpool) {
	// Create a descriptor pool size for every possible descriptor type. Works because they are
	// all just numbers (for example, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER = 6).

	// Corresponds to VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT
	#define MAX_DESCRIPTOR_TYPE 10
	VkDescriptorPoolSize sizes[MAX_DESCRIPTOR_TYPE + 1] = {0};
	for (int i = 0; i <= MAX_DESCRIPTOR_TYPE; i++) {
		sizes[i].type = (VkDescriptorType) i;
		for (int j = 0; j < desc_ct; j++) {
			if (descs[j].type == i) sizes[i].descriptorCount++;
		}
	}

	// A lot of those pools will have descriptorCount 0, which Vulkan doesn't like. So now we
	// take just the ones with descriptorCount > 0.
	VkDescriptorPoolSize used_sizes[MAX_DESCRIPTOR_TYPE + 1] = {0};
	int used_size_ct = 0;
	for (int i = 0; i <= MAX_DESCRIPTOR_TYPE; i++) {
		if (sizes[i].descriptorCount > 0) {
			memcpy(&used_sizes[used_size_ct], &sizes[i], sizeof(sizes[i]));
			used_size_ct++;
		}
	}

	VkDescriptorPoolCreateInfo dpool_info = {0};
	dpool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	dpool_info.maxSets = set_ct;
	dpool_info.poolSizeCount = used_size_ct;
	dpool_info.pPoolSizes = used_sizes;

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
		bindings[i].stageFlags = set_info->descs[i].stage;
	}

        VkDescriptorSetLayoutCreateInfo info = {0};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = set_info->desc_ct;
        info.pBindings = bindings;

        VkResult res = vkCreateDescriptorSetLayout(device, &info, NULL, layout);
        assert(res == VK_SUCCESS);

	free(bindings);
}

// `handles` must have set_info->desc_ct elements.
void set_create(VkDevice device, VkDescriptorPool dpool, VkDescriptorSetLayout layout,
		struct SetInfo* set_info, union SetHandle* handles, VkDescriptorSet *set)
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

		VkDescriptorType type = set_info->descs[i].type;
                if (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
		    || type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
                        writes[i].pBufferInfo = &handles[i].buffer;
                } else if (type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
                        writes[i].pImageInfo = &handles[i].image;
                }
        }

        vkUpdateDescriptorSets(device, set_info->desc_ct, writes, 0, NULL);

        free(writes);
}

#endif // LL_SET_H
