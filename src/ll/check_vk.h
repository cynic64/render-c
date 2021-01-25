#ifndef LL_CHECK_VK
#define LL_CHECK_VK

#include <vulkan/vulkan.h>

#include <stdio.h>

#ifndef NDEBUG
void check_vk(const char* message, VkResult result) {
        if (result == VK_SUCCESS) return;

	char err_buf[100];
	char* err_name = err_buf;
	sprintf(err_name, "%d", result);

        if (result == 1) err_name = "VK_NOT_READY";
        else if (result == 2) err_name = "VK_TIMEOUT";
        else if (result == 3) err_name = "VK_EVENT_SET";
        else if (result == 4) err_name = "VK_EVENT_RESET";
        else if (result == 5) err_name = "VK_INCOMPLETE";
        else if (result == -1) err_name = "VK_ERROR_OUT_OF_HOST_MEMORY";
        else if (result == -2) err_name = "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        else if (result == -3) err_name = "VK_ERROR_INITIALIZATION_FAILED";
        else if (result == -4) err_name = "VK_ERROR_DEVICE_LOST";
        else if (result == -5) err_name = "VK_ERROR_MEMORY_MAP_FAILED";
        else if (result == -6) err_name = "VK_ERROR_LAYER_NOT_PRESENT";
        else if (result == -7) err_name = "VK_ERROR_EXTENSION_NOT_PRESENT";
        else if (result == -8) err_name = "VK_ERROR_FEATURE_NOT_PRESENT";
        else if (result == -9) err_name = "VK_ERROR_INCOMPATIBLE_DRIVER";
        else if (result == -10) err_name = "VK_ERROR_TOO_MANY_OBJECTS";
        else if (result == -11) err_name = "VK_ERROR_FORMAT_NOT_SUPPORTED";
        else if (result == -12) err_name = "VK_ERROR_FRAGMENTED_POOL";
        else if (result == -13) err_name = "VK_ERROR_UNKNOWN";

        fprintf(stderr, "%s\n", message);
        fprintf(stderr, "Error code: %s\n", err_name);
        exit(1);
}
#else
void check_vk(const char* message, VkResult result) {}
#endif

#endif // LL_CHECK_VK

