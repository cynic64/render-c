#ifndef LL_SWAPCHAIN_H
#define LL_SWAPCHAIN_H

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
                      struct Swapchain* sc)
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
        sc->format = surface_format.format;

        sc->present_mode = present_modes[0];
        for (int i = 0; i < present_mode_ct; ++i) {
                if (present_modes[i] == present_mode_pref) sc->present_mode = present_modes[i];
        }

        sc->width = surface_caps.currentExtent.width;
        sc->height = surface_caps.currentExtent.height;
        assert(sc->width != UINT32_MAX && sc->height != UINT32_MAX);

        const uint32_t chosen_image_ct = surface_caps.maxImageCount > 0 ?
                surface_caps.maxImageCount : surface_caps.minImageCount;

        // Create swapchain
        VkSwapchainCreateInfoKHR sc_info = {0};
        sc_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        sc_info.surface = surface;
        sc_info.minImageCount = chosen_image_ct;
        sc_info.imageFormat = sc->format;
        sc_info.imageColorSpace = surface_format.colorSpace;
        sc_info.imageExtent.width = sc->width;
        sc_info.imageExtent.height = sc->height;
        sc_info.imageArrayLayers = 1;
        sc_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        sc_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        sc_info.preTransform = surface_caps.currentTransform;
        sc_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        sc_info.presentMode = sc->present_mode;
        sc_info.clipped = VK_TRUE;

        VkResult res = vkCreateSwapchainKHR(device, &sc_info, NULL, &sc->handle);
        assert(res == VK_SUCCESS);

        free(formats);
        free(present_modes);

        // Get images
        vkGetSwapchainImagesKHR(device, sc->handle, &sc->image_ct, NULL);
        sc->images = malloc(sc->image_ct * sizeof(sc->images[0]));
        vkGetSwapchainImagesKHR(device, sc->handle, &sc->image_ct, sc->images);

        // Create views
        sc->views = malloc(sc->image_ct * sizeof(sc->views[0]));
        for (int i = 0; i < sc->image_ct; ++i) {
                VkImageViewCreateInfo view_info = {0};
                view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                view_info.image = sc->images[i];
                view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
                view_info.format = sc->format;
                view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                view_info.subresourceRange.baseMipLevel = 0;
                view_info.subresourceRange.levelCount = 1;
                view_info.subresourceRange.baseArrayLayer = 0;
                view_info.subresourceRange.layerCount = 1;
                res = vkCreateImageView(device, &view_info, NULL, &sc->views[i]);
                assert(res == VK_SUCCESS);
        }
}

void swapchain_destroy(VkDevice device, struct Swapchain* sc) {
        for (int i = 0; i < sc->image_ct; ++i) vkDestroyImageView(device, sc->views[i], NULL);
        vkDestroySwapchainKHR(device, sc->handle, NULL);

        free(sc->images);
        free(sc->views);
}

#endif // LL_SWAPCHAIN_H

