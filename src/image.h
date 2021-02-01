#ifndef LL_IMAGE_H
#define LL_IMAGE_H

#include <vulkan/vulkan.h>

#include "cbuf.h"

#include <assert.h>
#include <stdio.h>

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
                       VkImageAspectFlags aspect, uint32_t mip_levels, VkImageView* view)
{
	VkImageViewCreateInfo info = {0};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	info.image = image;
	info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	info.format = format;
	info.subresourceRange.aspectMask = aspect;
	info.subresourceRange.baseMipLevel = 0;
	info.subresourceRange.levelCount = mip_levels;
	info.subresourceRange.baseArrayLayer = 0;
	info.subresourceRange.layerCount = 1;

	VkResult res = vkCreateImageView(device, &info, NULL, view);
	assert(res == VK_SUCCESS);
}

int image_check_format_supported(VkPhysicalDevice phys_dev, VkFormat format,
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
                  VkFormatFeatureFlags features, uint32_t mip_levels, VkSampleCountFlagBits samples,
                  struct Image* image)
{
        #ifndef NDEBUG
        if (!image_check_format_supported(phys_dev, format, tiling, features)) {
                fprintf(stderr, "Unsupported format with tiling %u: %u\n", tiling, format);
                exit(1);
        }
        #endif

        // Handle
	VkImageCreateInfo info = {0};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.imageType = VK_IMAGE_TYPE_2D;
	info.extent.width = width;
	info.extent.height = height;
	info.extent.depth = 1;
	info.mipLevels = mip_levels;
	info.arrayLayers = 1;
	info.format = format;
	info.tiling = tiling;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	info.usage = usage;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.samples = samples;

	VkResult res = vkCreateImage(device, &info, NULL, &image->handle);
	assert(res == VK_SUCCESS);

	// Memory
	VkMemoryRequirements mem_reqs;
	vkGetImageMemoryRequirements(device, image->handle, &mem_reqs);
	const uint32_t mem_idx = mem_type_idx_find(phys_dev, mem_reqs.memoryTypeBits, props);
	mem_alloc(device, mem_idx, mem_reqs.size, &image->mem);

	vkBindImageMemory(device, image->handle, image->mem, 0);

	// View
	image_view_create(device, image->handle, format, aspect, mip_levels, &image->view);
}

void image_destroy(VkDevice device, struct Image* image) {
	vkDestroyImage(device, image->handle, NULL);
	vkFreeMemory(device, image->mem, NULL);
	vkDestroyImageView(device, image->view, NULL);
}

void image_trans(VkDevice device, VkQueue queue, VkCommandPool cpool, VkImage image, VkImageAspectFlags aspect,
                 VkImageLayout old_lt, VkImageLayout new_lt, VkAccessFlags src_access, VkAccessFlags dst_access,
                 VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage, uint32_t mip_levels)
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
	barrier.subresourceRange.levelCount = mip_levels;
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

void framebuffer_create(VkDevice device, VkRenderPass rpass, uint32_t width, uint32_t height,
                        uint32_t attachment_count, const VkImageView* views,
                        VkFramebuffer* framebuffer)
{
        VkFramebufferCreateInfo info = {0};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = rpass;
        info.attachmentCount = attachment_count;
        info.pAttachments = views;
        info.width = width;
        info.height = height;
        info.layers = 1;

        VkResult res = vkCreateFramebuffer(device, &info, NULL, framebuffer);
        assert(res == VK_SUCCESS);
}

void image_create_depth(VkPhysicalDevice phys_dev, VkDevice device,
                        VkFormat format, uint32_t width, uint32_t height, VkSampleCountFlagBits samples,
                        struct Image* image)
{
	image_create(phys_dev, device, format, width, height,
	             VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	             VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
	             VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, 1, samples, image);
}

void image_create_color(VkPhysicalDevice phys_dev, VkDevice device,
                        VkFormat format, uint32_t width, uint32_t height, VkSampleCountFlagBits samples,
                        struct Image* image)
{
	image_create(phys_dev, device, format, width, height,
	             VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	             VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
	             0, 1, samples, image);
}

#endif // LL_IMAGE_H

