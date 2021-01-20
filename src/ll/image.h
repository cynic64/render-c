#ifndef LL_IMAGE_H
#define LL_IMAGE_H

#include <vulkan/vulkan.h>

#include "cbuf.h"

#include <assert.h>

struct Image {
        VkImage handle;
        VkImageView view;
        VkDeviceMemory mem;
};

// Optional settings
struct ImageExtraInfo {
        VkImageTiling tiling;
        VkImageAspectFlags aspect;
        VkImageLayout layout;
        VkMemoryPropertyFlags props;
        VkFormatFeatureFlags features;
};

void image_view_create(VkDevice device, VkImage image, VkFormat format,
                       VkImageAspectFlags aspect, VkImageView* view)
{
	VkImageViewCreateInfo info = {0};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	info.image = image;
	info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	info.format = format;
	info.subresourceRange.aspectMask = aspect;
	info.subresourceRange.baseMipLevel = 0;
	info.subresourceRange.levelCount = 1;
	info.subresourceRange.baseArrayLayer = 0;
	info.subresourceRange.layerCount = 1;

	VkResult res = vkCreateImageView(device, &info, NULL, view);
	assert(res == VK_SUCCESS);
}

int format_supported(VkPhysicalDevice phys_dev, VkFormat format,
                     VkImageTiling tiling, VkFormatFeatureFlags features)
{
	VkFormatProperties props;
	vkGetPhysicalDeviceFormatProperties(phys_dev, format, &props);

	assert(tiling == VK_IMAGE_TILING_OPTIMAL || tiling == VK_IMAGE_TILING_LINEAR);
	if (tiling == VK_IMAGE_TILING_OPTIMAL) return (props.optimalTilingFeatures & features) == features;
	else if (tiling == VK_IMAGE_TILING_LINEAR) return (props.linearTilingFeatures & features) == features;
	else return 0;
}

void image_create(VkPhysicalDevice phys_dev, VkDevice device, VkFormat format, uint32_t width, uint32_t height,
                  VkImageTiling tiling, VkImageAspectFlags aspect,
                  VkMemoryPropertyFlags props, VkImageUsageFlags usage,
                  VkFormatFeatureFlags features, struct Image* image)
{
        // Handle
        assert(format_supported(phys_dev, format, tiling, features));

	VkImageCreateInfo info = {0};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.imageType = VK_IMAGE_TYPE_2D;
	info.extent.width = width;
	info.extent.height = height;
	info.extent.depth = 1;
	info.mipLevels = 1;
	info.arrayLayers = 1;
	info.format = format;
	info.tiling = tiling;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	info.usage = usage;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.samples = VK_SAMPLE_COUNT_1_BIT;

	VkResult res = vkCreateImage(device, &info, NULL, &image->handle);
	assert(res == VK_SUCCESS);

	// Memory
	VkMemoryRequirements mem_reqs;
	vkGetImageMemoryRequirements(device, image->handle, &mem_reqs);
	const uint32_t mem_idx = mem_type_idx_find(phys_dev, mem_reqs.memoryTypeBits, props);
	mem_alloc(device, mem_idx, mem_reqs.size, &image->mem);

	vkBindImageMemory(device, image->handle, image->mem, 0);

	// View
	image_view_create(device, image->handle, format, aspect, &image->view);
}

void image_destroy(VkDevice device, struct Image* image) {
	vkDestroyImage(device, image->handle, NULL);
	vkFreeMemory(device, image->mem, NULL);
	vkDestroyImageView(device, image->view, NULL);
}

void image_trans(VkDevice device, VkQueue queue, VkCommandPool cpool, VkImage image, VkImageAspectFlags aspect,
                 VkImageLayout old_lt, VkImageLayout new_lt, VkAccessFlags src_access, VkAccessFlags dst_access,
                 VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage)
{
	VkImageMemoryBarrier barrier = {0};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = old_lt;
	barrier.newLayout = new_lt;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = aspect;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = src_access;
	barrier.dstAccessMask = dst_access;

	VkCommandBuffer cbuf;
	cbuf_alloc(device, cpool, &cbuf);
	cbuf_begin_onetime(cbuf);
	vkCmdPipelineBarrier(cbuf, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &barrier);
	cbuf_submit_wait(queue, cbuf);
	vkFreeCommandBuffers(device, cpool, 1, &cbuf);
}

// Assumes image is already VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
void image_copy_from_buffer(VkDevice device, VkQueue queue, VkCommandPool cpool, VkImageAspectFlags aspect,
                            VkBuffer src, VkImage dst, uint32_t width, uint32_t height)
{
	VkBufferImageCopy region = {0};
	region.imageSubresource.aspectMask = aspect;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageExtent = (VkExtent3D){width, height, 1};

	VkCommandBuffer cbuf;
	cbuf_alloc(device, cpool, &cbuf);
	cbuf_begin_onetime(cbuf);
	vkCmdCopyBufferToImage(cbuf, src, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	cbuf_submit_wait(queue, cbuf);
	vkFreeCommandBuffers(device, cpool, 1, &cbuf);
}

#endif // LL_IMAGE_H

