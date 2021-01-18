#ifndef LL_BUFFER_H
#define LL_BUFFER_H

#include <vulkan/vulkan.h>

#include "cbuf.h"

#include <assert.h>
#include <string.h>

struct Buffer {
        VkBuffer handle;
        VkDeviceMemory mem;
        VkDeviceSize size;
};

void buffer_handle_create(VkDevice device, VkBufferUsageFlags usage, VkDeviceSize size, VkBuffer* buf) {
        VkBufferCreateInfo info = {0};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size = size;
        info.usage = usage;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkResult res = vkCreateBuffer(device, &info, NULL, buf);
        assert(res == VK_SUCCESS);
}

void buffer_mem_alloc(VkDevice device, uint32_t mem_type_idx, VkDeviceSize size, VkDeviceMemory* mem) {
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

void buffer_create(VkPhysicalDevice phys_dev, VkDevice device,
                   VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkDeviceSize size, 
                   struct Buffer* buf)
{
        buffer_handle_create(device, usage, size, &buf->handle);

        VkMemoryRequirements mem_reqs;
        vkGetBufferMemoryRequirements(device, buf->handle, &mem_reqs);

	uint32_t mem_type_idx = mem_type_idx_find(phys_dev, mem_reqs.memoryTypeBits, props);
	buffer_mem_alloc(device, mem_type_idx, mem_reqs.size, &buf->mem);

        vkBindBufferMemory(device, buf->handle, buf->mem, 0);

        buf->size = size;
}

void buffer_destroy(VkDevice device, struct Buffer* buf) {
        vkDestroyBuffer(device, buf->handle, NULL);
        vkFreeMemory(device, buf->mem, NULL);
}

void buffer_mem_write(VkDevice device, VkDeviceMemory mem, VkDeviceSize size, const void* data) {
        void *mapped;
        vkMapMemory(device, mem, 0, size, 0, &mapped);
	memcpy(mapped, data, size);
        vkUnmapMemory(device, mem);
}

void buffer_copy(VkQueue queue, VkCommandBuffer cbuf, VkBuffer src, VkBuffer dst, VkDeviceSize size) {
        cbuf_begin_onetime(cbuf);

        VkBufferCopy copy_region = {0};
        copy_region.size = size;
        vkCmdCopyBuffer(cbuf, src, dst, 1, &copy_region);

        vkEndCommandBuffer(cbuf);

        cbuf_submit_wait(queue, cbuf);
}

void buffer_staged(VkPhysicalDevice phys_dev, VkDevice device, VkQueue queue, VkCommandPool cpool,
                   VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
                   VkDeviceSize size, const void* data, struct Buffer* buf)
{
        struct Buffer staging;
        buffer_create(phys_dev, device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      size, &staging);
        buffer_mem_write(device, staging.mem, size, data);

	VkBufferUsageFlags real_usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        buffer_create(phys_dev, device, real_usage, props, size, buf);

        VkCommandBuffer cbuf;
        cbuf_alloc(device, cpool, &cbuf);
        buffer_copy(queue, cbuf, staging.handle, buf->handle, size);

        buffer_destroy(device, &staging);
        vkFreeCommandBuffers(device, cpool, 1, &cbuf);
}

#endif // LL_BUFFER_H

