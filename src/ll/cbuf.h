#ifndef LL_CBUF_H
#define LL_CBUF_H

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

#endif // LL_CBUF_H

