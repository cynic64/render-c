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

void cbuf_barrier_image(VkCommandBuffer cbuf, VkImage image, VkImageAspectFlags aspect,
                        uint32_t mip_level_ct, uint32_t mip_level_base,
                        VkImageLayout old_layout, VkImageLayout new_layout,
                        VkAccessFlags src_access, VkAccessFlags dst_access,
                        VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage)
{
	VkImageMemoryBarrier barrier = {0};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = image;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = aspect;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = mip_level_ct;
	barrier.subresourceRange.baseMipLevel = mip_level_base;
	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.srcAccessMask = src_access;
	barrier.dstAccessMask = dst_access;
	vkCmdPipelineBarrier(cbuf, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &barrier);
}

#endif // LL_CBUF_H

