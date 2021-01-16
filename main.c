#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

const int VALIDATION_LAYER_CT = 1;
const char* VALIDATION_LAYERS[] = {"VK_LAYER_KHRONOS_validation"};

const int INSTANCE_EXT_CT = 1;
const char* INSTANCE_EXTS[] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};

const int DEVICE_EXT_CT = 1;
const char* DEVICE_EXTS[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

const VkFormat SC_FORMAT_PREF = VK_FORMAT_B8G8R8A8_SRGB;
const VkPresentModeKHR SC_PRESENT_MODE_PREF = VK_PRESENT_MODE_IMMEDIATE_KHR;

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_cback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                  VkDebugUtilsMessageTypeFlagsEXT type,
                                                  const VkDebugUtilsMessengerCallbackDataEXT* cback_data,
                                                  void* user_data)
{
        fprintf(stderr, "%s\n", cback_data->pMessage);
        return VK_FALSE;
}

VkShaderModule load_shader(VkDevice device, const char* path) {
        FILE *fp = fopen(path, "r");
        fseek(fp, 0L, SEEK_END);
        const int byte_ct = ftell(fp);
        rewind(fp);

        char* buf = malloc(byte_ct);
        fread(buf, 1, byte_ct, fp);
        fclose(fp);

        VkShaderModuleCreateInfo info = {0};
        info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.codeSize = byte_ct;
        info.pCode = (const uint32_t*) buf;

        VkShaderModule shader;
        VkResult res = vkCreateShaderModule(device, &info, NULL, &shader);
        assert(res == VK_SUCCESS);

        free(buf);

	return shader;
}

int main() {
        // GLFW
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan", NULL, NULL);

        // Combine GLFW extensions with whatever we want
        uint32_t glfw_ext_ct = 0;
        const char** glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_ext_ct);

        uint32_t instance_ext_ct = glfw_ext_ct + INSTANCE_EXT_CT;
        const char** instance_exts = malloc(instance_ext_ct * sizeof(instance_exts[0]));
        for (int i = 0; i < instance_ext_ct; ++i) {
                if (i < glfw_ext_ct) instance_exts[i] = glfw_exts[i];
                else instance_exts[i] = INSTANCE_EXTS[i-glfw_ext_ct];
        }

        // Query extensions
        uint32_t avail_instance_ext_ct = 0;
        vkEnumerateInstanceExtensionProperties(NULL, &avail_instance_ext_ct, NULL);
        VkExtensionProperties* avail_instance_exts = malloc(avail_instance_ext_ct * sizeof(avail_instance_exts[0]));
        vkEnumerateInstanceExtensionProperties(NULL, &avail_instance_ext_ct, avail_instance_exts);
        for (int i = 0; i < instance_ext_ct; ++i) {
                const char* ext_want = instance_exts[i];
                int found = 0;
                for (int j = 0; j < avail_instance_ext_ct; ++j) {
                        if (strcmp(avail_instance_exts[j].extensionName, ext_want) == 0) found = 1;
                }
                assert(found);
        }

        // Query validation layers
        uint32_t layer_ct;
        vkEnumerateInstanceLayerProperties(&layer_ct, NULL);
        VkLayerProperties* layers = malloc(layer_ct * sizeof(layers[0]));
        vkEnumerateInstanceLayerProperties(&layer_ct, layers);

        for (int i = 0; i < VALIDATION_LAYER_CT; ++i) {
                const char* want_layer = VALIDATION_LAYERS[i];
                int found = 0;
                for (int j = 0; j < layer_ct && !found; ++j) {
                        if (strcmp(layers[j].layerName, want_layer) == 0) found = 1;
                }
                assert(found);
        }

        // Fill in debug messenger info
        VkDebugUtilsMessengerCreateInfoEXT debug_info = {0};
        debug_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
 	// VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT excluded
        debug_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debug_info.pfnUserCallback = debug_cback;

        // Instance
        VkApplicationInfo app_info = {0};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "Thing";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.pEngineName = "No Engine";
        app_info.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo instance_info = {0};
        instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_info.pApplicationInfo = &app_info;
        instance_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debug_info;

        instance_info.enabledLayerCount = VALIDATION_LAYER_CT;
        instance_info.ppEnabledLayerNames = VALIDATION_LAYERS;

        instance_info.enabledExtensionCount = instance_ext_ct;
        instance_info.ppEnabledExtensionNames = instance_exts;

        VkInstance instance;
        VkResult res = vkCreateInstance(&instance_info, NULL, &instance);
        assert(res == VK_SUCCESS);

        // Surface
        VkSurfaceKHR surface;
        res = glfwCreateWindowSurface(instance, window, NULL, &surface);
        assert(res == VK_SUCCESS);

        // Create debug messenger
        VkDebugUtilsMessengerEXT debug_msgr;
        PFN_vkCreateDebugUtilsMessengerEXT debug_create_fun =
        	(PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        assert(debug_create_fun != NULL);
        debug_create_fun(instance, &debug_info, NULL, &debug_msgr);

        // Physical device
	uint32_t phys_dev_ct = 0;
	vkEnumeratePhysicalDevices(instance, &phys_dev_ct, NULL);
	VkPhysicalDevice* phys_devs = malloc(phys_dev_ct * sizeof(phys_devs[0]));
	vkEnumeratePhysicalDevices(instance, &phys_dev_ct, phys_devs);
	VkPhysicalDevice phys_dev = phys_devs[0];

	VkPhysicalDeviceProperties phys_dev_props;
	vkGetPhysicalDeviceProperties(phys_dev, &phys_dev_props);
	printf("Using device: %s\n", phys_dev_props.deviceName);

	// Queue families
	uint32_t queue_fam_ct = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(phys_dev, &queue_fam_ct, NULL);
	VkQueueFamilyProperties* queue_fam_props = malloc(queue_fam_ct * sizeof(queue_fam_props[0]));
	vkGetPhysicalDeviceQueueFamilyProperties(phys_dev, &queue_fam_ct, queue_fam_props);
	uint32_t queue_fam = UINT32_MAX;
	for (int i = 0; i < queue_fam_ct && queue_fam == UINT32_MAX; ++i) {
        	int graphics_ok = queue_fam_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;
        	VkBool32 present_ok = VK_FALSE;
        	vkGetPhysicalDeviceSurfaceSupportKHR(phys_dev, i, surface, &present_ok);
        	if (graphics_ok && present_ok) queue_fam = i;
	}
	assert(queue_fam != UINT32_MAX);

	// Query device extensions
	uint32_t dev_ext_ct = 0;
	vkEnumerateDeviceExtensionProperties(phys_dev, NULL, &dev_ext_ct, NULL);
	VkExtensionProperties* dev_exts = malloc(dev_ext_ct * sizeof(dev_exts[0]));
	vkEnumerateDeviceExtensionProperties(phys_dev, NULL, &dev_ext_ct, dev_exts);
	for (int i = 0; i < DEVICE_EXT_CT; ++i) {
        	const char * want_ext = DEVICE_EXTS[i];
        	int found = 0;
        	for (int j = 0; j < dev_ext_ct && !found; ++j) {
                	if (strcmp(dev_exts[j].extensionName, want_ext) == 0) found = 1;
        	}
        	assert(found);
	}

	// Create logical device
	const float queue_priority = 1.0F;
	VkDeviceQueueCreateInfo dev_queue_info = {0};
	dev_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	dev_queue_info.queueFamilyIndex = queue_fam;
	dev_queue_info.queueCount = 1;
	dev_queue_info.pQueuePriorities = &queue_priority;

	VkPhysicalDeviceFeatures dev_features = {0};

	VkDeviceCreateInfo device_info = {0};
	device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	device_info.pQueueCreateInfos = &dev_queue_info;
	device_info.queueCreateInfoCount = 1;
	device_info.pEnabledFeatures = &dev_features;
	device_info.enabledLayerCount = 0;
	device_info.enabledExtensionCount = DEVICE_EXT_CT;
	device_info.ppEnabledExtensionNames = DEVICE_EXTS;

	VkDevice device;
	res = vkCreateDevice(phys_dev, &device_info, NULL, &device);
	assert(res == VK_SUCCESS);

	// Create queue
	VkQueue queue;
	vkGetDeviceQueue(device, queue_fam, 0, &queue);

	// Choose swapchain settings
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

	VkSurfaceFormatKHR sc_surface_format = formats[0];
	for (int i = 0; i < format_ct; ++i) {
                if (formats[i].format == SC_FORMAT_PREF) sc_surface_format = formats[i];
	}
	const VkFormat sc_format = sc_surface_format.format;

	VkPresentModeKHR sc_present_mode = present_modes[0];
	for (int i = 0; i < present_mode_ct; ++i) {
                if (present_modes[i] == SC_PRESENT_MODE_PREF) sc_present_mode = present_modes[i];
	}

	const uint32_t swidth = surface_caps.maxImageExtent.width, sheight = surface_caps.maxImageExtent.height;
	assert(swidth != UINT32_MAX && sheight != UINT32_MAX);

	const uint32_t chosen_image_ct = surface_caps.maxImageCount > 0 ?
		surface_caps.maxImageCount : surface_caps.minImageCount;

	// Create swapchain
	VkSwapchainCreateInfoKHR sc_info = {0};
	sc_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	sc_info.surface = surface;
	sc_info.minImageCount = chosen_image_ct;
	sc_info.imageFormat = sc_format;
	sc_info.imageColorSpace = sc_surface_format.colorSpace;
	sc_info.imageExtent.width = swidth;
	sc_info.imageExtent.height = sheight;
	sc_info.imageArrayLayers = 1;
	sc_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	sc_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	sc_info.preTransform = surface_caps.currentTransform;
	sc_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	sc_info.presentMode = sc_present_mode;
	sc_info.clipped = VK_TRUE;

	VkSwapchainKHR swapchain;
	res = vkCreateSwapchainKHR(device, &sc_info, NULL, &swapchain);
	assert(res == VK_SUCCESS);

	// Get images
	uint32_t image_ct = 0; // Not necessarily what we chose
	vkGetSwapchainImagesKHR(device, swapchain, &image_ct, NULL);
	VkImage* images = malloc(image_ct * sizeof(images[0]));
	vkGetSwapchainImagesKHR(device, swapchain, &image_ct, images);

	VkImageView* image_views = malloc(image_ct * sizeof(image_views[0]));
	for (int i = 0; i < image_ct; ++i) {
        	VkImageViewCreateInfo view_info = {0};
        	view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        	view_info.image = images[i];
        	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        	view_info.format = sc_format;
        	view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        	view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        	view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        	view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        	view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        	view_info.subresourceRange.baseMipLevel = 0;
        	view_info.subresourceRange.levelCount = 1;
        	view_info.subresourceRange.baseArrayLayer = 0;
        	view_info.subresourceRange.layerCount = 1;
        	res = vkCreateImageView(device, &view_info, NULL, &image_views[i]);
        	assert(res == VK_SUCCESS);
	}

	// Load shaders
	VkShaderModule vs = load_shader(device, "shader.vs.spv");
	VkShaderModule fs = load_shader(device, "shader.vs.spv");

	// Render pass
	VkAttachmentDescription attach_desc = {0};
	attach_desc.format = sc_format;
	attach_desc.samples = VK_SAMPLE_COUNT_1_BIT;
	attach_desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attach_desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attach_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attach_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attach_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attach_desc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference attach_ref = {0};
	attach_ref.attachment = 0;
	attach_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {0};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &attach_ref;

	VkRenderPassCreateInfo rpass_info = {0};
	rpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	rpass_info.attachmentCount = 1;
	rpass_info.pAttachments = &attach_desc;
	rpass_info.subpassCount = 1;
	rpass_info.pSubpasses = &subpass;

	VkRenderPass rpass;
	res = vkCreateRenderPass(device, &rpass_info, NULL, &rpass);
	assert(res == VK_SUCCESS);

	// Main loop
        while (!glfwWindowShouldClose(window)) {
                glfwPollEvents();
        }

	// Cleanup
	vkDestroyRenderPass(device, rpass, NULL);

	vkDestroyShaderModule(device, vs, NULL);
	vkDestroyShaderModule(device, fs, NULL);

	for (int i = 0; i < image_ct; ++i) vkDestroyImageView(device, image_views[i], NULL);

	vkDestroySwapchainKHR(device, swapchain, NULL);

	vkDestroyDevice(device, NULL);

        PFN_vkDestroyDebugUtilsMessengerEXT debug_destroy_fun =
        	(PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        assert(debug_destroy_fun != NULL);
        debug_destroy_fun(instance, debug_msgr, NULL);

        vkDestroySurfaceKHR(instance, surface, NULL);

        vkDestroyInstance(instance, NULL);

        glfwDestroyWindow(window);
        glfwTerminate();
}
