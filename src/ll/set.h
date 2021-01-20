#ifndef LL_SET_H
#define LL_SET_H

#include <vulkan/vulkan.h>

#include <assert.h>
#include <stdlib.h>

struct Descriptor {
        VkDescriptorType type;
        uint32_t binding;
        VkShaderStageFlags shader_stage_flags;
        // Only for buffers
        VkDescriptorBufferInfo buffer;
        // Only for images
        VkDescriptorImageInfo image;
};

void set_create(VkDevice device, VkDescriptorPool dpool, VkDescriptorSetLayout layout,
                uint32_t descriptor_ct, const struct Descriptor* descriptors,
                VkDescriptorSet* set)
{
        VkDescriptorSetAllocateInfo set_info = {0};
        set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        set_info.descriptorPool = dpool;
        set_info.descriptorSetCount = 1;
        set_info.pSetLayouts = &layout;

        VkResult res = vkAllocateDescriptorSets(device, &set_info, set);
        assert(res == VK_SUCCESS);

        VkWriteDescriptorSet* writes = malloc(descriptor_ct * sizeof(writes[0]));
        for (int i = 0; i < descriptor_ct; ++i) {
                writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[i].dstSet = *set;
                writes[i].dstBinding = descriptors[i].binding;
                writes[i].dstArrayElement = 0;
                writes[i].descriptorType = descriptors[i].type;
                writes[i].descriptorCount = 1;

                if (descriptors[i].type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
                        writes[i].pBufferInfo = &descriptors[i].buffer;
                } else if (descriptors[i].type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
                        writes[i].pImageInfo = &descriptors[i].image;
                }
        }

        vkUpdateDescriptorSets(device, descriptor_ct, writes, 0, NULL);

        free(writes);
}

#endif // LL_SET_H
