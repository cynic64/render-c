#ifndef LL_MEM_H
#define LL_MEM_H

#include <vulkan/vulkan.h>

#include <assert.h>

void mem_alloc(VkDevice device, uint32_t mem_type_idx, VkDeviceSize size, VkDeviceMemory* mem) {
        VkMemoryAllocateInfo info = {0};
        info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        info.allocationSize = size;
        info.memoryTypeIndex = mem_type_idx;

        VkResult res = vkAllocateMemory(device, &info, NULL, mem);
        assert(res == VK_SUCCESS);
}

uint32_t mem_type_idx_find(VkPhysicalDevice phys_dev, uint32_t idx_mask, VkMemoryPropertyFlags props) {
        VkPhysicalDeviceMemoryProperties phys_dev_mems;
        vkGetPhysicalDeviceMemoryProperties(phys_dev, &phys_dev_mems);

        uint32_t chosen = UINT32_MAX;
        for (uint32_t i = 0; i < phys_dev_mems.memoryTypeCount && chosen == UINT32_MAX; ++i) {
                int type_ok = idx_mask & (1 << i);
                int props_ok = (props & phys_dev_mems.memoryTypes[i].propertyFlags) == props;
                if (type_ok && props_ok) chosen = i;
        }

        assert(chosen != UINT32_MAX);
        return chosen;
}

void mem_write(VkDevice device, VkDeviceMemory mem, VkDeviceSize size, const void* data) {
        void *mapped;
        vkMapMemory(device, mem, 0, size, 0, &mapped);
	memcpy(mapped, data, size);
        vkUnmapMemory(device, mem);
}

#endif // LL_MEM_H

