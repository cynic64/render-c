#ifndef LL_CBUF_H
#define LL_CBUF_H

#include <vulkan/vulkan.h>

#include <assert.h>

void cbuf_alloc(VkDevice device, VkCommandPool cpool, VkCommandBuffer* cbuf) {
        VkCommandBufferAllocateInfo info = {0};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        info.commandPool = cpool;
        info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        info.commandBufferCount = 1;

        VkResult res = vkAllocateCommandBuffers(device, &info, cbuf);
        assert(res == VK_SUCCESS);
}

void cbuf_begin_onetime(VkCommandBuffer cbuf) {
        VkCommandBufferBeginInfo info = {0};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cbuf, &info);
}

void cbuf_submit_wait(VkQueue queue, VkCommandBuffer cbuf) {
        vkEndCommandBuffer(cbuf);

        VkSubmitInfo info = {0};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.commandBufferCount = 1;
        info.pCommandBuffers = &cbuf;

        vkQueueSubmit(queue, 1, &info, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue);
}

#endif // LL_CBUF_H

