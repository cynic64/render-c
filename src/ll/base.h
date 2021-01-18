#ifndef LL_BASE_H
#define LL_BASE_H

#include <vulkan/vulkan.h>

#include <assert.h>
#include <string.h>

const int VALIDATION_LAYER_CT = 1;
const char* VALIDATION_LAYERS[] = {"VK_LAYER_KHRONOS_validation"};

struct Base {
	VkInstance instance;
	VkDebugUtilsMessengerEXT dbg_msgr;
	VkSurfaceKHR surface;
	VkPhysicalDevice phys_dev;
	const char* phys_dev_name;
	uint32_t queue_fam;
	VkDevice device;
	VkQueue queue;
};

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_cback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                  VkDebugUtilsMessageTypeFlagsEXT type,
                                                  const VkDebugUtilsMessengerCallbackDataEXT* cback_data,
                                                  void* user_data)
{
        (void)(severity);
        (void)(type);
        (void)(user_data);
        fprintf(stderr, "%s\n", cback_data->pMessage);
        return VK_FALSE;
}

void base_create(GLFWwindow* window,
                 int want_debug,
                 uint32_t instance_ext_ct, const char** instance_exts, 
                 uint32_t device_ext_ct, const char** device_exts,
                 struct Base* base)
{
        // Combine GLFW extensions with whatever user wants
        uint32_t glfw_ext_ct = 0;
        const char** glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_ext_ct);

	uint32_t real_instance_ext_ct = instance_ext_ct + glfw_ext_ct;
        const char** real_instance_exts = malloc(real_instance_ext_ct * sizeof(real_instance_exts[0]));
        for (int i = 0; i < real_instance_ext_ct; ++i) {
                if (i < glfw_ext_ct) real_instance_exts[i] = glfw_exts[i];
                else real_instance_exts[i] = instance_exts[i-glfw_ext_ct];
        }

        // Query extensions
        uint32_t avail_instance_ext_ct = 0;
        vkEnumerateInstanceExtensionProperties(NULL, &avail_instance_ext_ct, NULL);
        VkExtensionProperties* avail_instance_exts = malloc(avail_instance_ext_ct * sizeof(avail_instance_exts[0]));
        vkEnumerateInstanceExtensionProperties(NULL, &avail_instance_ext_ct, avail_instance_exts);
        for (int i = 0; i < real_instance_ext_ct; ++i) {
                const char* ext_want = real_instance_exts[i];
                int found = 0;
                for (int j = 0; j < avail_instance_ext_ct && !found; ++j) {
                        if (strcmp(avail_instance_exts[j].extensionName, ext_want) == 0) found = 1;
                }
                assert(found);
        }

        free(avail_instance_exts);

        // (Maybe) setup debug messenger and check validation layers
        VkDebugUtilsMessengerCreateInfoEXT debug_info = {0};

        if (want_debug) {
                uint32_t layer_ct = 0;
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

                free(layers);

                // Fill in debug messenger info
                debug_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
                // VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT excluded
                debug_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
                debug_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
                debug_info.pfnUserCallback = debug_cback;
        }

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

	if (want_debug) {
                instance_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debug_info;
                instance_info.enabledLayerCount = VALIDATION_LAYER_CT;
                instance_info.ppEnabledLayerNames = VALIDATION_LAYERS;
	} else instance_info.enabledLayerCount = 0;

        instance_info.enabledExtensionCount = real_instance_ext_ct;
        instance_info.ppEnabledExtensionNames = real_instance_exts;

        VkResult res = vkCreateInstance(&instance_info, NULL, &base->instance);
        assert(res == VK_SUCCESS);

        free(real_instance_exts);

        // Surface
        res = glfwCreateWindowSurface(base->instance, window, NULL, &base->surface);
        assert(res == VK_SUCCESS);

        // (Maybe) Create debug messenger
        if (want_debug) {
                PFN_vkCreateDebugUtilsMessengerEXT debug_create_fun = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(base->instance, "vkCreateDebugUtilsMessengerEXT");
                assert(debug_create_fun != NULL);
                debug_create_fun(base->instance, &debug_info, NULL, &base->dbg_msgr);
        } else base->dbg_msgr = VK_NULL_HANDLE;

        // Physical device
        uint32_t phys_dev_ct = 0;
        vkEnumeratePhysicalDevices(base->instance, &phys_dev_ct, NULL);
        VkPhysicalDevice* phys_devs = malloc(phys_dev_ct * sizeof(phys_devs[0]));
        vkEnumeratePhysicalDevices(base->instance, &phys_dev_ct, phys_devs);
        base->phys_dev = phys_devs[0];
        free(phys_devs);

        VkPhysicalDeviceProperties phys_dev_props;
        vkGetPhysicalDeviceProperties(base->phys_dev, &phys_dev_props);
        base->phys_dev_name = phys_dev_props.deviceName;

        // Queue families
        uint32_t queue_fam_ct = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(base->phys_dev, &queue_fam_ct, NULL);
        VkQueueFamilyProperties* queue_fam_props = malloc(queue_fam_ct * sizeof(queue_fam_props[0]));
        vkGetPhysicalDeviceQueueFamilyProperties(base->phys_dev, &queue_fam_ct, queue_fam_props);
        uint32_t queue_fam = UINT32_MAX;
        for (int i = 0; i < queue_fam_ct && queue_fam == UINT32_MAX; ++i) {
                int graphics_ok = queue_fam_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;
                VkBool32 present_ok = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(base->phys_dev, i, base->surface, &present_ok);
                if (graphics_ok && present_ok) queue_fam = i;
        }
        assert(queue_fam != UINT32_MAX);
        base->queue_fam = queue_fam;
        free(queue_fam_props);

        // Query device extensions
        uint32_t real_dev_ext_ct = 0;
        vkEnumerateDeviceExtensionProperties(base->phys_dev, NULL, &real_dev_ext_ct, NULL);
        VkExtensionProperties* real_dev_exts = malloc(real_dev_ext_ct * sizeof(real_dev_exts[0]));
        vkEnumerateDeviceExtensionProperties(base->phys_dev, NULL, &real_dev_ext_ct, real_dev_exts);
        for (int i = 0; i < device_ext_ct; ++i) {
                const char * want_ext = device_exts[i];
                int found = 0;
                for (int j = 0; j < real_dev_ext_ct && !found; ++j) {
                        if (strcmp(real_dev_exts[j].extensionName, want_ext) == 0) found = 1;
                }
                assert(found);
        }
        free(real_dev_exts);

        // Create logical device
        const float queue_priority = 1.0F;
        VkDeviceQueueCreateInfo dev_queue_info = {0};
        dev_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        dev_queue_info.queueFamilyIndex = base->queue_fam;
        dev_queue_info.queueCount = 1;
        dev_queue_info.pQueuePriorities = &queue_priority;

        VkPhysicalDeviceFeatures dev_features = {0};

        VkDeviceCreateInfo device_info = {0};
        device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_info.pQueueCreateInfos = &dev_queue_info;
        device_info.queueCreateInfoCount = 1;
        device_info.pEnabledFeatures = &dev_features;
        device_info.enabledLayerCount = 0;
        device_info.enabledExtensionCount = device_ext_ct;
        device_info.ppEnabledExtensionNames = device_exts;

        res = vkCreateDevice(base->phys_dev, &device_info, NULL, &base->device);
        assert(res == VK_SUCCESS);

        // Create queue
        vkGetDeviceQueue(base->device, base->queue_fam, 0, &base->queue);
}

void base_destroy(struct Base* base) {
	vkDeviceWaitIdle(base->device);

        vkDestroyDevice(base->device, NULL);

        vkDestroySurfaceKHR(base->instance, base->surface, NULL);

	if (base->dbg_msgr != VK_NULL_HANDLE) {
                PFN_vkDestroyDebugUtilsMessengerEXT dbg_destroy_fun = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(base->instance, "vkDestroyDebugUtilsMessengerEXT");
                assert(dbg_destroy_fun != NULL);
                dbg_destroy_fun(base->instance, base->dbg_msgr, NULL);
	}

        vkDestroyInstance(base->instance, NULL);
}

#endif // LL_BASE_H