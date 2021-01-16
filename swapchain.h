#ifndef swapchain_h_INCLUDED
#define swapchain_h_INCLUDED

#include <vulkan/vulkan.h>
#include <assert.h>
#include <stdlib.h>

struct Swapchain {
        VkSwapchainKHR handle;

        VkFormat format;
        VkPresentModeKHR present_mode;
        uint32_t width;
        uint32_t height;

        uint32_t image_ct;
        VkImage* images;
        VkImageView* views;
};

void swapchain_create(VkSurfaceKHR surface, VkPhysicalDevice phys_dev, VkDevice device,
                      VkFormat format_pref, VkPresentModeKHR present_mode_pref,
                      struct Swapchain* swapchain)
{
	// Choose settings
	VkSurfaceCapabilitiesKHR surface_caps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_dev, surface, &surface_caps);

	uint32_t format_ct = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(phys_dev, surface, &format_ct, NULL);
	VkSurfaceFormatKHR* formats = malloc(format_ct * sizeof(formats[0]));
	vkGetPhysicalDeviceSurfaceFormatsKHR(phys_dev, surface, &format_ct, formats);

	uint32_t present_mode_ct = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(phys_dev, surface, &present_mode_ct, NULL);
	VkPresentModeKHR* present_modes = malloc(present_mode_ct * sizeof(present_modes[0]));
	vkGetPhysicalDeviceSurfacePresentModesKHR(phys_dev, surface, &present_mode_ct, present_modes);

	VkSurfaceFormatKHR surface_format = formats[0];
	for (int i = 0; i < format_ct; ++i) {
                if (formats[i].format == format_pref) surface_format = formats[i];
	}
	swapchain->format = surface_format.format;

	swapchain->present_mode = present_modes[0];
	for (int i = 0; i < present_mode_ct; ++i) {
                if (present_modes[i] == present_mode_pref) swapchain->present_mode = present_modes[i];
	}

	swapchain->width = surface_caps.maxImageExtent.width;
	swapchain->height = surface_caps.maxImageExtent.height;
	assert(swapchain->width != UINT32_MAX && swapchain->height != UINT32_MAX);

	const uint32_t chosen_image_ct = surface_caps.maxImageCount > 0 ?
		surface_caps.maxImageCount : surface_caps.minImageCount;

	// Create swapchain
	VkSwapchainCreateInfoKHR sc_info = {0};
	sc_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	sc_info.surface = surface;
	sc_info.minImageCount = chosen_image_ct;
	sc_info.imageFormat = swapchain->format;
	sc_info.imageColorSpace = surface_format.colorSpace;
	sc_info.imageExtent.width = swapchain->width;
	sc_info.imageExtent.height = swapchain->height;
	sc_info.imageArrayLayers = 1;
	sc_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	sc_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	sc_info.preTransform = surface_caps.currentTransform;
	sc_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	sc_info.presentMode = swapchain->present_mode;
	sc_info.clipped = VK_TRUE;

	VkResult res = vkCreateSwapchainKHR(device, &sc_info, NULL, &swapchain->handle);
	assert(res == VK_SUCCESS);

	free(formats);
	free(present_modes);
}

#endif // swapchain_h_INCLUDED

