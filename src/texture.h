#ifndef TEXTURE_H
#define TEXTURE_H

/*
 * stb_image.h must already be included.
 */

#include <vulkan/vulkan.h>

#include "ll/buffer.h"
#include "ll/image.h"
#include "ll/set.h"

#include <stdint.h>
#include <stdio.h>

const VkFormat TEXTURE_FORMAT = VK_FORMAT_B8G8R8A8_SRGB;
const char* const TEXTURE_FALLBACK_PATH = "assets/error.png";

void texture_set_from_path(VkPhysicalDevice phys_dev, VkDevice device, VkQueue queue,
                           VkCommandPool cpool, VkDescriptorPool dpool, VkSampler sampler,
                           VkDescriptorSetLayout layout, const char* path,
                           struct Image* texture, VkDescriptorSet* set)
{
        int width, height, channels;
	stbi_uc* pixels = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);

	if (pixels == NULL) {
        	fprintf(stderr, "Could not load %s!\n", path);
        	pixels = stbi_load(TEXTURE_FALLBACK_PATH, &width, &height, &channels, STBI_rgb_alpha);
        	assert(pixels != NULL);
	}

	int tex_size = 4 * width * height;

	// Create source buffer
	struct Buffer tex_buf;
	buffer_create(phys_dev, device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	              tex_size, &tex_buf);
	mem_write(device, tex_buf.mem, tex_size, pixels);
	stbi_image_free(pixels);

	// Create texture
	image_create(phys_dev, device, VK_FORMAT_R8G8B8A8_SRGB, width, height,
	             VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		     VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT, texture);

	// Copy to texture
	image_trans(device, queue, cpool, texture->handle, VK_IMAGE_ASPECT_COLOR_BIT,
	            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	            0, VK_ACCESS_TRANSFER_WRITE_BIT,
	            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	image_copy_from_buffer(device, queue, cpool, VK_IMAGE_ASPECT_COLOR_BIT,
	                       tex_buf.handle, texture->handle, width, height);

	image_trans(device, queue, cpool, texture->handle, VK_IMAGE_ASPECT_COLOR_BIT,
	            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
	            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	buffer_destroy(device, &tex_buf);

	// Create set
        struct Descriptor desc_tex = {0};
        desc_tex.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        desc_tex.binding = 0;
        desc_tex.shader_stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
        desc_tex.image.sampler = sampler;
        desc_tex.image.imageView = texture->view;
        desc_tex.image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        set_create(device, dpool, layout, 1, &desc_tex, set);
}

#endif // TEXTURE_H
