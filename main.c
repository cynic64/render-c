#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

const int VALIDATION_LAYER_CT = 1;
const char* VALIDATION_LAYERS[] = {"VK_LAYER_KHRONOS_validation"};

const int INSTANCE_EXT_CT = 1;
const char* INSTANCE_EXTS[] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};

const int DEVICE_EXT_CT = 1;
const char* DEVICE_EXTS[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

const VkFormat SC_FORMAT_PREF = VK_FORMAT_B8G8R8A8_SRGB;
const VkPresentModeKHR SC_PRESENT_MODE_PREF = VK_PRESENT_MODE_IMMEDIATE_KHR;

const int CBUF_CT = 4;

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
        const int read_bytes = fread(buf, 1, byte_ct, fp);
        assert(read_bytes == byte_ct);
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
	VkShaderModule fs = load_shader(device, "shader.fs.spv");

	VkPipelineShaderStageCreateInfo shaders[2] = {0};
	shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaders[0].module = vs;
	shaders[0].pName = "main";
	shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaders[1].module = fs;
	shaders[1].pName = "main";

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

	VkSubpassDependency subpass_dep = {0};
	subpass_dep.srcSubpass = VK_SUBPASS_EXTERNAL;
	subpass_dep.dstSubpass = 0;
	subpass_dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpass_dep.srcAccessMask = 0;
	subpass_dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpass_dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo rpass_info = {0};
	rpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	rpass_info.attachmentCount = 1;
	rpass_info.pAttachments = &attach_desc;
	rpass_info.subpassCount = 1;
	rpass_info.pSubpasses = &subpass;
	rpass_info.dependencyCount = 1;
	rpass_info.pDependencies = &subpass_dep;

	VkRenderPass rpass;
	res = vkCreateRenderPass(device, &rpass_info, NULL, &rpass);
	assert(res == VK_SUCCESS);

	// Pipeline layout
	VkPipelineLayoutCreateInfo pipeline_lt_info = {0};
	pipeline_lt_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	VkPipelineLayout pipeline_lt;
	res = vkCreatePipelineLayout(device, &pipeline_lt_info, NULL, &pipeline_lt);
	assert(res == VK_SUCCESS);

	// Pipeline
	VkPipelineVertexInputStateCreateInfo input_info = {0};
	input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	VkPipelineInputAssemblyStateCreateInfo input_assembly = {0};
	input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly.primitiveRestartEnable = VK_FALSE;

	VkPipelineRasterizationStateCreateInfo rasterizer = {0};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0F;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewport_state = {0};
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.scissorCount = 1;

	VkPipelineMultisampleStateCreateInfo multisampling = {0};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState color_blend_attach = {0};
	color_blend_attach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	color_blend_attach.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo color_blend = {0};
	color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blend.logicOpEnable = VK_FALSE;
	color_blend.attachmentCount = 1;
	color_blend.pAttachments = &color_blend_attach;

	VkDynamicState dyn_states[] = {VK_DYNAMIC_STATE_VIEWPORT,
	                               VK_DYNAMIC_STATE_SCISSOR};

	VkPipelineDynamicStateCreateInfo dyn_state_info = {0};
	dyn_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dyn_state_info.dynamicStateCount = sizeof(dyn_states) / sizeof(dyn_states[0]);
	dyn_state_info.pDynamicStates = dyn_states;

	VkGraphicsPipelineCreateInfo pipeline_info = {0};
	pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_info.stageCount = sizeof(shaders) / sizeof(shaders[0]);
	pipeline_info.pStages = shaders;
	pipeline_info.pVertexInputState = &input_info;
	pipeline_info.pInputAssemblyState = &input_assembly;
	pipeline_info.pRasterizationState = &rasterizer;
	pipeline_info.pViewportState = &viewport_state;
	pipeline_info.pMultisampleState = &multisampling;
	pipeline_info.pColorBlendState = &color_blend;
	pipeline_info.pDynamicState = &dyn_state_info;
	pipeline_info.layout = pipeline_lt;
	pipeline_info.renderPass = rpass;
	pipeline_info.subpass = 0;

	VkPipeline pipeline;
	res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline);
	assert(res == VK_SUCCESS);

	// Framebuffers
	VkFramebuffer* fbs = malloc(image_ct * sizeof(fbs[0]));
	for (int i = 0; i < image_ct; ++i) {
    		VkImageView attachments[] = {image_views[i]};

    		VkFramebufferCreateInfo fb_info = {0};
    		fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    		fb_info.renderPass = rpass;
    		fb_info.attachmentCount = 1;
    		fb_info.pAttachments = attachments;
    		fb_info.width = swidth;
    		fb_info.height = sheight;
    		fb_info.layers = 1;

    		res = vkCreateFramebuffer(device, &fb_info, NULL, &fbs[i]);
    		assert(res == VK_SUCCESS);
	}

	// Command pool
	VkCommandPoolCreateInfo cpool_info = {0};
	cpool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cpool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	cpool_info.queueFamilyIndex = queue_fam;

	VkCommandPool cpool;
	res = vkCreateCommandPool(device, &cpool_info, NULL, &cpool);
	assert(res == VK_SUCCESS);

	// Allocate command buffers
	VkCommandBufferAllocateInfo cbuf_info = {0};
	cbuf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbuf_info.commandPool = cpool;
	cbuf_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cbuf_info.commandBufferCount = CBUF_CT;

	VkCommandBuffer* cbufs = malloc(CBUF_CT * sizeof(cbufs[0]));
	res = vkAllocateCommandBuffers(device, &cbuf_info, cbufs);
	assert(res == VK_SUCCESS);

	// Sync objects
	VkFence* render_done_fences = malloc(CBUF_CT * sizeof(render_done_fences[0]));
	VkSemaphore* image_avail_sems = malloc(CBUF_CT * sizeof(image_avail_sems[0]));
	VkSemaphore* render_done_sems = malloc(CBUF_CT * sizeof(render_done_sems[0]));

	for (int i = 0; i < CBUF_CT; ++i) {
        	VkFenceCreateInfo fence_info = {0};
        	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        	vkCreateFence(device, &fence_info, NULL, &render_done_fences[i]);

		VkSemaphoreCreateInfo sem_info = {0};
		sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		vkCreateSemaphore(device, &sem_info, NULL, &image_avail_sems[i]);
		vkCreateSemaphore(device, &sem_info, NULL, &render_done_sems[i]);
	}
	VkFence* image_fences = malloc(image_ct * sizeof(image_fences[0]));
	for (int i = 0; i < image_ct; ++i) image_fences[i] = VK_NULL_HANDLE;

	// Main loop
	int frame_ct = 0;
	struct timespec start_time;
	clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);
        while (!glfwWindowShouldClose(window)) {
                int sync_idx = frame_ct % CBUF_CT;

                VkFence render_done_fence = render_done_fences[sync_idx];
                VkSemaphore image_avail_sem = image_avail_sems[sync_idx];
                VkSemaphore render_done_sem = render_done_sems[sync_idx];

                // Wait for the frame using these sync objects to finish rendering
                res = vkWaitForFences(device, 1, &render_done_fence, VK_TRUE, UINT64_MAX);
                assert(res == VK_SUCCESS);

                // Reset command buffer
                VkCommandBuffer cbuf = cbufs[sync_idx];
                vkResetCommandBuffer(cbuf, 0);

                // Acquire an image
                uint32_t image_idx;
                vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, image_avail_sem, VK_NULL_HANDLE, &image_idx);

                // Record command buffer
                VkCommandBufferBeginInfo cbuf_begin_info = {0};
                cbuf_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                cbuf_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                vkBeginCommandBuffer(cbuf, &cbuf_begin_info);

		VkClearValue clear_color = {0.0F, 0.0F, 0.0F, 1.0F};

                VkRenderPassBeginInfo cbuf_rpass_info = {0};
                cbuf_rpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                cbuf_rpass_info.renderPass = rpass;
                cbuf_rpass_info.framebuffer = fbs[image_idx];
		cbuf_rpass_info.renderArea.extent.width = swidth;
		cbuf_rpass_info.renderArea.extent.height = sheight;
		cbuf_rpass_info.clearValueCount = 1;
		cbuf_rpass_info.pClearValues = &clear_color;
		vkCmdBeginRenderPass(cbuf, &cbuf_rpass_info, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(cbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		VkViewport viewport = {0};
		viewport.width = swidth;
		viewport.height = sheight;
		viewport.minDepth = 0.0F;
		viewport.maxDepth = 1.0F;
		vkCmdSetViewport(cbuf, 0, 1, &viewport);

		VkRect2D scissor = {0};
		scissor.extent.width = swidth;
		scissor.extent.height = sheight;
		vkCmdSetScissor(cbuf, 0, 1, &scissor);

		vkCmdDraw(cbuf, 3, 1, 0, 0);
		vkCmdEndRenderPass(cbuf);

		res = vkEndCommandBuffer(cbuf);
		assert(res == VK_SUCCESS);

                // Wait until whoever is rendering to the image is done
                if (image_fences[image_idx] != VK_NULL_HANDLE)
                        vkWaitForFences(device, 1, &image_fences[image_idx], VK_TRUE, UINT64_MAX);

		// Reset fence
                res = vkResetFences(device, 1, &render_done_fence);
                assert(res == VK_SUCCESS);

		// Mark it as in use by us
		image_fences[image_idx] = render_done_fence;

		// Submit
		VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo submit_info = {0};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &image_avail_sem;
		submit_info.pWaitDstStageMask = &wait_stage;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &cbuf;
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &render_done_sem;

		res = vkQueueSubmit(queue, 1, &submit_info, render_done_fence);
		assert(res == VK_SUCCESS);

		// Present
		VkPresentInfoKHR present_info = {0};
		present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present_info.waitSemaphoreCount = 1;
		present_info.pWaitSemaphores = &render_done_sem;
		present_info.swapchainCount = 1;
		present_info.pSwapchains = &swapchain;
		present_info.pImageIndices = &image_idx;

		res = vkQueuePresentKHR(queue, &present_info);
		assert(res == VK_SUCCESS);

                glfwPollEvents();
                ++frame_ct;
        }

        struct timespec stop_time;
        clock_gettime(CLOCK_MONOTONIC_RAW, &stop_time);
        double elapsed = (double) ((stop_time.tv_sec * 1000000000 + stop_time.tv_nsec)
        	- (start_time.tv_sec * 1000000000 + start_time.tv_nsec)) / 1000000000.0D;
        double fps = (double) frame_ct / elapsed;
        printf("FPS: %.2f\n", fps);

	// Cleanup
	vkDeviceWaitIdle(device);

	for (int i = 0; i < CBUF_CT; ++i) {
        	vkDestroyFence(device, render_done_fences[i], NULL);
        	vkDestroySemaphore(device, image_avail_sems[i], NULL);
        	vkDestroySemaphore(device, render_done_sems[i], NULL);
	}

	vkDestroyPipeline(device, pipeline, NULL);

	vkDestroyPipelineLayout(device, pipeline_lt, NULL);

	vkDestroyCommandPool(device, cpool, NULL);
	vkDestroyRenderPass(device, rpass, NULL);

	vkDestroyShaderModule(device, vs, NULL);
	vkDestroyShaderModule(device, fs, NULL);

	for (int i = 0; i < image_ct; ++i) {
        	rpass_info.dependencyCount = 1;
        	rpass_info.pDependencies = &subpass_dep;
        	vkDestroyImageView(device, image_views[i], NULL);
        	vkDestroyFramebuffer(device, fbs[i], NULL);
	}

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
