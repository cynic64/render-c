#ifndef LL_SYNC_H
#define LL_SYNC_H

#include <vulkan/vulkan.h>

void fence_create(VkDevice device, VkFenceCreateFlags flags, VkFence* fence) {
        VkFenceCreateInfo info = {0};
        info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        info.flags = flags;

        VkResult res = vkCreateFence(device, &info, NULL, fence);
        assert(res == VK_SUCCESS);
}

void semaphore_create(VkDevice device, VkSemaphore* sem) {
        VkSemaphoreCreateInfo info = {0};
        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkResult res = vkCreateSemaphore(device, &info, NULL, sem);
        assert(res == VK_SUCCESS);
}

#endif // LL_SYNC_H

