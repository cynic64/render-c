#ifndef LL_RENDER_PROC_H
#define LL_RENDER_PROC_H

#include "ll/cbuf.h"
#include "ll/sync.h"

struct RenderProc {
        VkCommandBuffer cbuf;
        VkFence fence;
        VkSemaphore acquire_sem;
        VkSemaphore render_sem;
};

void render_proc_create_sync(VkDevice device, struct RenderProc* rproc) {
        fence_create(device, VK_FENCE_CREATE_SIGNALED_BIT, &rproc->fence);
        semaphore_create(device, &rproc->acquire_sem);
        semaphore_create(device, &rproc->render_sem);
}

void render_proc_destroy_sync(VkDevice device, struct RenderProc* rproc) {
        vkDestroyFence(device, rproc->fence, NULL);
        vkDestroySemaphore(device, rproc->acquire_sem, NULL);
        vkDestroySemaphore(device, rproc->render_sem, NULL);
}

void render_proc_create(VkDevice device, VkCommandPool cpool, struct RenderProc* rproc) {
        cbuf_alloc(device, cpool, &rproc->cbuf);
        render_proc_create_sync(device, rproc);
}

#endif // LL_RENDER_PROC_H
