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

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_cback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                  VkDebugUtilsMessageTypeFlagsEXT type,
                                                  const VkDebugUtilsMessengerCallbackDataEXT* cback_data,
                                                  void* user_data)
{
        fprintf(stderr, "%s\n", cback_data->pMessage);
        return VK_FALSE;
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

        // Create debug messenger
        VkDebugUtilsMessengerEXT debug_msgr;
        PFN_vkCreateDebugUtilsMessengerEXT debug_create_fun =
        	(PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        assert(debug_create_fun != NULL);
        debug_create_fun(instance, &debug_info, NULL, &debug_msgr);

	// Main loop
        while (!glfwWindowShouldClose(window)) {
                glfwPollEvents();
        }

	// Cleanup
        PFN_vkDestroyDebugUtilsMessengerEXT debug_destroy_fun =
        	(PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        assert(debug_destroy_fun != NULL);
        debug_destroy_fun(instance, debug_msgr, NULL);

        vkDestroyInstance(instance, NULL);

        glfwDestroyWindow(window);
        glfwTerminate();
}
