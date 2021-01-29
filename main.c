#include "external/cglm/include/cglm/cglm.h"
#define STBI_ONLY_TGA
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_FAILURE_USERMSG
#define STB_IMAGE_IMPLEMENTATION
#include "external/stb_image/stb_image.h"
#define FAST_OBJ_IMPLEMENTATION
#include "external/fast_obj/fast_obj.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include "src/camera.h"
#include "src/obj.h"
#include "src/texture.h"
#include "src/timer.h"

#include "src/ll/base.h"
#include "src/ll/buffer.h"
#include "src/ll/glfw_error.h"
#include "src/ll/image.h"
#include "src/ll/pipeline.h"
#include "src/ll/set.h"
#include "src/ll/shader.h"
#include "src/ll/swapchain.h"
#include "src/ll/sync.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

const int INSTANCE_EXT_CT = 1;
const char* INSTANCE_EXTS[] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};

const char* DEVICE_EXTS[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

const int DEVICE_EXT_CT = 1;
const VkFormat SC_FORMAT_PREF = VK_FORMAT_B8G8R8A8_SRGB;
const VkPresentModeKHR SC_PRESENT_MODE_PREF = VK_PRESENT_MODE_IMMEDIATE_KHR;
const VkFormat DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;

const double MOUSE_SENSITIVITY_FACTOR = 0.001;
const float MOVEMENT_SPEED = 50.0F;

#define CONCURRENT_FRAMES 4

#ifndef NDEBUG
        const int VALIDATION_ON = 1;
#else
	const int VALIDATION_ON = 0;
#endif

struct Vertex {
        vec3 pos;
        vec3 norm;
        vec2 tex_c;
};

struct CameraUniform {
        mat4 view;
        mat4 proj;
};

struct PushConstants {
        mat4 model;
        vec4 ambient;
        vec4 diffuse;
        vec4 specular;
        float use_textures;
};

struct SyncSet {
        VkFence render_fence;
        VkSemaphore acquire_sem;
        VkSemaphore render_sem;
};

struct Mesh {
        uint32_t index_offset;
        uint32_t index_ct;
        VkDescriptorSet set;
        struct PushConstants push_constants;
};

void sync_set_create(VkDevice device, struct SyncSet* sync_set) {
        fence_create(device, VK_FENCE_CREATE_SIGNALED_BIT, &sync_set->render_fence);
        semaphore_create(device, &sync_set->acquire_sem);
        semaphore_create(device, &sync_set->render_sem);
}

void sync_set_destroy(VkDevice device, struct SyncSet* sync_set) {
	vkDestroyFence(device, sync_set->render_fence, NULL);
	vkDestroySemaphore(device, sync_set->acquire_sem, NULL);
	vkDestroySemaphore(device, sync_set->render_sem, NULL);
}

int main(int argc, char** argv) {
        assert(argc == 2);
        const char* mesh_path = argv[1];

        struct timespec time_prog_start = timer_start();

        // GLFW
        glfwInit();
        glfwSetErrorCallback(glfw_error_callback);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan", NULL, NULL);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // Base
        struct Base base;
        base_create(window, VALIDATION_ON, INSTANCE_EXT_CT, INSTANCE_EXTS, DEVICE_EXT_CT, DEVICE_EXTS, &base);

        const VkSampleCountFlagBits sample_ct = base.max_samples > VK_SAMPLE_COUNT_4_BIT ?
       		VK_SAMPLE_COUNT_4_BIT : base.max_samples;

        // Load model
        fastObjMesh* model = fast_obj_read(mesh_path);
        assert(model != NULL);
        uint32_t vertex_ct, index_ct, mesh_ct;
        obj_convert_model(model, &vertex_ct, NULL, &index_ct, NULL, &mesh_ct, NULL);

        struct ObjVertex* vertices_raw = malloc(vertex_ct * sizeof(vertices_raw[0]));
        uint32_t* indices = malloc(index_ct * sizeof(indices[0]));
        struct ObjMesh* meshes_raw = malloc(mesh_ct * sizeof(meshes_raw[0]));
        obj_convert_model(model, &vertex_ct, vertices_raw, &index_ct, indices, &mesh_ct, meshes_raw);

        struct Vertex* vertices = malloc(vertex_ct * sizeof(vertices[0]));
        struct Mesh* meshes = malloc(mesh_ct * sizeof(meshes[0]));
        for (int i = 0; i < vertex_ct; i++) {
                vertices[i].pos[0] = vertices_raw[i].pos[0];
                vertices[i].pos[1] = vertices_raw[i].pos[1];
                vertices[i].pos[2] = vertices_raw[i].pos[2];
                vertices[i].norm[0] = vertices_raw[i].norm[0];
                vertices[i].norm[1] = vertices_raw[i].norm[1];
                vertices[i].norm[2] = vertices_raw[i].norm[2];
                vertices[i].tex_c[0] = vertices_raw[i].tex_c[0];
                vertices[i].tex_c[1] = vertices_raw[i].tex_c[1];
        }
        for (int i = 0; i < mesh_ct; i++) {
                meshes[i].index_offset = meshes_raw[i].index_offset;
                meshes[i].index_ct = meshes_raw[i].index_ct;

                glm_mat4_identity(meshes[i].push_constants.model);
                meshes[i].push_constants.ambient[0] = model->materials[i].Ka[0];
                meshes[i].push_constants.ambient[1] = model->materials[i].Ka[1];
                meshes[i].push_constants.ambient[2] = model->materials[i].Ka[2];
                meshes[i].push_constants.diffuse[0] = model->materials[i].Kd[0];
                meshes[i].push_constants.diffuse[1] = model->materials[i].Kd[1];
                meshes[i].push_constants.diffuse[2] = model->materials[i].Kd[2];
                meshes[i].push_constants.specular[0] = model->materials[i].Ks[0];
                meshes[i].push_constants.specular[1] = model->materials[i].Ks[1];
                meshes[i].push_constants.specular[2] = model->materials[i].Ks[2];
        }
        free(vertices_raw);
        free(meshes_raw);

        printf("[%.3fs] Loaded mesh (%u vertices, %u indices)\n",
               timer_get_elapsed(&time_prog_start), vertex_ct, index_ct);

        // Vertex/index buffer
        struct Buffer vbuf, ibuf;
        buffer_staged(base.phys_dev, base.device, base.queue, base.cpool, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertex_ct * sizeof(vertices[0]), vertices, &vbuf);
        buffer_staged(base.phys_dev, base.device, base.queue, base.cpool, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, index_ct * sizeof(indices[0]), indices, &ibuf);

        printf("[%.3fs] Copied mesh to GPU\n", timer_get_elapsed(&time_prog_start));

	// Create sampler
	VkPhysicalDeviceProperties phys_dev_props;
	vkGetPhysicalDeviceProperties(base.phys_dev, &phys_dev_props);

	VkSamplerCreateInfo sampler_info = {0};
	sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_info.magFilter = VK_FILTER_LINEAR;
	sampler_info.minFilter = VK_FILTER_LINEAR;
	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.anisotropyEnable = VK_TRUE;
	sampler_info.maxAnisotropy = phys_dev_props.limits.maxSamplerAnisotropy;
	sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	sampler_info.unnormalizedCoordinates = VK_FALSE;
	sampler_info.compareEnable = VK_FALSE;
	sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_info.maxLod = 1000.0f;

	VkSampler tex_sampler;
	VkResult res = vkCreateSampler(base.device, &sampler_info, NULL, &tex_sampler);
	assert(res == VK_SUCCESS);

        // Descriptor pool
        VkDescriptorPoolSize dpool_sizes[2] = {0};
        dpool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        dpool_sizes[0].descriptorCount = CONCURRENT_FRAMES;
        dpool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        dpool_sizes[1].descriptorCount = mesh_ct;

        VkDescriptorPoolCreateInfo dpool_info = {0};
        dpool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        dpool_info.poolSizeCount = sizeof(dpool_sizes) / sizeof(dpool_sizes[0]);
        dpool_info.pPoolSizes = dpool_sizes;
        dpool_info.maxSets = CONCURRENT_FRAMES + mesh_ct;

        VkDescriptorPool dpool;
        res = vkCreateDescriptorPool(base.device, &dpool_info, NULL, &dpool);
        assert(res == VK_SUCCESS);

        // Load textures and create texture set for each object
       	VkDescriptorSetLayoutBinding set_tex_desc = {0};
        set_tex_desc.binding = 0;
        set_tex_desc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        set_tex_desc.descriptorCount = 1;
        set_tex_desc.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayout set_tex_layout;
        set_layout_create(base.device, 1, &set_tex_desc, &set_tex_layout);

	struct Image* textures = malloc(mesh_ct * sizeof(textures[0]));
	for (int i = 0; i < mesh_ct; i++) {
        	const char* path = model->materials[i].map_Kd.path;
        	if (path == NULL) {
                	path = TEXTURE_FALLBACK_PATH;
                	meshes[i].push_constants.use_textures = 0.0F;
        	} else meshes[i].push_constants.use_textures = 1.0F;

        	texture_set_from_path(base.phys_dev, base.device, base.queue, base.cpool, dpool, tex_sampler,
                                      set_tex_layout, path, &textures[i], &meshes[i].set);
	};

        printf("[%.3fs] Loaded textures\n", timer_get_elapsed(&time_prog_start));

	// Cleanup
	fast_obj_destroy(model);
        free(vertices);
        free(indices);

        // Create uniform buffers
       	VkDescriptorSetLayoutBinding set_cam_desc = {0};
        set_cam_desc.binding = 0;
        set_cam_desc.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        set_cam_desc.descriptorCount = 1;
        set_cam_desc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        VkDescriptorSetLayout set_cam_layout;
        set_layout_create(base.device, 1, &set_cam_desc, &set_cam_layout);

        // Uniform buffers (one for every render process)
        struct Buffer ubufs[CONCURRENT_FRAMES];
        for (int i = 0; i < CONCURRENT_FRAMES; i++) {
                buffer_create(base.phys_dev, base.device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                              sizeof(struct CameraUniform), &ubufs[i]);
        };

        // Uniform sets
        VkDescriptorSet sets_cam[CONCURRENT_FRAMES];
        for (int i = 0; i < CONCURRENT_FRAMES; i++) {
                struct Descriptor desc_cam = {0};
                desc_cam.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                desc_cam.binding = 0;
                desc_cam.shader_stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
                desc_cam.buffer.buffer = ubufs[i].handle;
                desc_cam.buffer.range = ubufs[i].size;
                set_create(base.device, dpool, set_cam_layout,
                           1, &desc_cam, &sets_cam[i]);
        }

	// Camera
	struct CameraFly camera;
	camera.pitch = 0.0F;
	camera.yaw = 0.0F;
	camera.eye[0] = 0.0F; camera.eye[1] = 2.0F; camera.eye[2] = 0.0F; 
	double last_mouse_x, last_mouse_y;
	glfwGetCursorPos(window, &last_mouse_x, &last_mouse_y);

        // Swapchain
        struct Swapchain swapchain;
        swapchain_create(base.surface, base.phys_dev, base.device, SC_FORMAT_PREF, SC_PRESENT_MODE_PREF, &swapchain);

        // Vertex input
        VkVertexInputBindingDescription vtx_bind_desc = {0};
        vtx_bind_desc.binding = 0;
        vtx_bind_desc.stride = sizeof(struct Vertex);
        vtx_bind_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription vtx_attr_descs[3] = {0};
        vtx_attr_descs[0].binding = 0;
        vtx_attr_descs[0].location = 0;
        vtx_attr_descs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        vtx_attr_descs[0].offset = offsetof(struct Vertex, pos);

        vtx_attr_descs[1].binding = 0;
        vtx_attr_descs[1].location = 1;
        vtx_attr_descs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        vtx_attr_descs[1].offset = offsetof(struct Vertex, norm);

        vtx_attr_descs[2].binding = 0;
        vtx_attr_descs[2].location = 2;
        vtx_attr_descs[2].format = VK_FORMAT_R32G32_SFLOAT;
        vtx_attr_descs[2].offset = offsetof(struct Vertex, tex_c);

        // Load shaders
        VkShaderModule vs = load_shader(base.device, "shaders/shader.vs.spv");
        VkShaderModule fs = load_shader(base.device, "shaders/shader.fs.spv");

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
        VkAttachmentDescription attachments[3] = {0};
        // Multisampled color
        attachments[0].format = swapchain.format;
        attachments[0].samples = sample_ct;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// Multisampled depth
        attachments[1].format = DEPTH_FORMAT;
        attachments[1].samples = sample_ct;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // Color resolve
        attachments[2].format = swapchain.format;
        attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[2].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference color_multisampled_ref = {0};
        color_multisampled_ref.attachment = 0;
        color_multisampled_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depth_ref = {0};
        depth_ref.attachment = 1;
        depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_resolve_ref = {0};
        color_resolve_ref.attachment = 2;
        color_resolve_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {0};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_multisampled_ref;
        subpass.pDepthStencilAttachment = &depth_ref;
        subpass.pResolveAttachments = &color_resolve_ref;

        VkSubpassDependency subpass_dep = {0};
        subpass_dep.srcSubpass = VK_SUBPASS_EXTERNAL;
        subpass_dep.dstSubpass = 0;
        subpass_dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        	                 | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        subpass_dep.srcAccessMask = 0;
        subpass_dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        	                 | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        subpass_dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                                  | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo rpass_info = {0};
        rpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpass_info.attachmentCount = sizeof(attachments) / sizeof(attachments[0]);
        rpass_info.pAttachments = attachments;
        rpass_info.subpassCount = 1;
        rpass_info.pSubpasses = &subpass;
        rpass_info.dependencyCount = 1;
        rpass_info.pDependencies = &subpass_dep;

        VkRenderPass rpass;
        res = vkCreateRenderPass(base.device, &rpass_info, NULL, &rpass);
        assert(res == VK_SUCCESS);

        // Push constants
        assert(sizeof(struct PushConstants) <= BASE_MAX_PUSH_CONSTANT_SIZE);
	VkPushConstantRange push_constant_range = {0};
	push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	push_constant_range.offset = 0;
	push_constant_range.size = sizeof(struct PushConstants);

        // Pipeline layout
	VkDescriptorSetLayout pipeline_set_layouts[] = {set_cam_layout, set_tex_layout};

        VkPipelineLayoutCreateInfo pipeline_layout_info = {0};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = sizeof(pipeline_set_layouts) / sizeof(pipeline_set_layouts[0]);
        pipeline_layout_info.pSetLayouts = pipeline_set_layouts;
        pipeline_layout_info.pushConstantRangeCount = 1;
        pipeline_layout_info.pPushConstantRanges = &push_constant_range;

        VkPipelineLayout pipeline_layout;
        res = vkCreatePipelineLayout(base.device, &pipeline_layout_info, NULL, &pipeline_layout);
        assert(res == VK_SUCCESS);

	for (int i = 0; i < sizeof(pipeline_set_layouts) / sizeof(pipeline_set_layouts[0]); i++) {
                vkDestroyDescriptorSetLayout(base.device, pipeline_set_layouts[i], NULL);
	}

        // Pipeline
        VkPipelineVertexInputStateCreateInfo vertex_info = {0};
        vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_info.vertexBindingDescriptionCount = 1;
        vertex_info.pVertexBindingDescriptions = &vtx_bind_desc;
        vertex_info.vertexAttributeDescriptionCount = sizeof(vtx_attr_descs) / sizeof(vtx_attr_descs[0]);
        vertex_info.pVertexAttributeDescriptions = vtx_attr_descs;

        struct PipelineSettings pipeline_settings = PIPELINE_SETTINGS_DEFAULT;
        pipeline_settings.depth.depthTestEnable = VK_TRUE;
        pipeline_settings.depth.depthWriteEnable = VK_TRUE;
        pipeline_settings.depth.depthCompareOp = VK_COMPARE_OP_LESS;

        pipeline_settings.multisampling.rasterizationSamples = sample_ct;

	VkPipeline pipeline;
	pipeline_create(base.device, &pipeline_settings,
	                sizeof(shaders) / sizeof(shaders[0]), shaders, &vertex_info,
	                pipeline_layout, rpass, 0, &pipeline);

	// Depth buffer
	struct Image depth_image;
	image_create_depth(base.phys_dev, base.device, DEPTH_FORMAT,
                           swapchain.width, swapchain.height, sample_ct, &depth_image);

	// Color image (multisampled if supported)
	struct Image color_image;
	image_create_color(base.phys_dev, base.device, swapchain.format,
                           swapchain.width, swapchain.height, sample_ct, &color_image);

        // Framebuffers
        VkFramebuffer* framebuffers = malloc(swapchain.image_ct * sizeof(framebuffers[0]));
        for (int i = 0; i < swapchain.image_ct; i++) {
                VkImageView views[] = {color_image.view, depth_image.view, swapchain.views[i]};
                framebuffer_create(base.device, rpass, swapchain.width, swapchain.height,
                                   sizeof(views) / sizeof(views[0]), views, &framebuffers[i]);
        }

        // Command buffers
        VkCommandBuffer cbufs[CONCURRENT_FRAMES];
        for (int i = 0; i < CONCURRENT_FRAMES; i++) cbuf_alloc(base.device, base.cpool, &cbufs[i]);

        // Sync sets
        struct SyncSet sync_sets [CONCURRENT_FRAMES];
        for (int i = 0; i < CONCURRENT_FRAMES; i++) sync_set_create(base.device, &sync_sets[i]);

        // Image fences
        VkFence* image_fences = malloc(swapchain.image_ct * sizeof(image_fences[0]));
        for (int i = 0; i < swapchain.image_ct; i++) image_fences[i] = VK_NULL_HANDLE;

        printf("[%.3fs] Begin main loop\n", timer_get_elapsed(&time_prog_start));

        // Main loop
        int frame_ct = 0;
        struct timespec start_time = timer_start();
        struct timespec last_frame_time = timer_start();
        int must_recreate = 0;

        while (!glfwWindowShouldClose(window)) {
                while (must_recreate) {
                        must_recreate = 0;
                        vkDeviceWaitIdle(base.device);

                        VkFormat old_format = swapchain.format;
                        uint32_t old_image_ct = swapchain.image_ct;

                        swapchain_destroy(base.device, &swapchain);
                        swapchain_create(base.surface, base.phys_dev, base.device,
                                         SC_FORMAT_PREF, SC_PRESENT_MODE_PREF, &swapchain);

                        assert(swapchain.format == old_format && swapchain.image_ct == old_image_ct);

                        for (int i = 0; i < CONCURRENT_FRAMES; i++) {
                                sync_set_destroy(base.device, &sync_sets[i]);
                                sync_set_create(base.device, &sync_sets[i]);
                        }

			// Recreate depth
                        image_destroy(base.device, &depth_image);
                	image_create_depth(base.phys_dev, base.device, DEPTH_FORMAT,
                                           swapchain.width, swapchain.height,
                                           sample_ct, &depth_image);

                	// Recreate color
                        image_destroy(base.device, &color_image);
                	image_create_color(base.phys_dev, base.device, swapchain.format,
                                           swapchain.width, swapchain.height,
                                           sample_ct, &color_image);

                        for (int i = 0; i < swapchain.image_ct; i++) {
                                vkDestroyFramebuffer(base.device, framebuffers[i], NULL);

                                VkImageView new_fb_views[] = {color_image.view, depth_image.view,
                                                              swapchain.views[i]};
                                framebuffer_create(base.device, rpass, swapchain.width, swapchain.height,
                                                   sizeof(new_fb_views) / sizeof(new_fb_views[0]),
                                                   new_fb_views, &framebuffers[i]);

                                image_fences[i] = VK_NULL_HANDLE;
                        }

                        int real_width, real_height;
                        glfwGetFramebufferSize(window, &real_width, &real_height);
                        if (real_width != swapchain.width || real_height != swapchain.height) must_recreate = 1;
                }

                // Handle input
                // Mouse movement
                double new_mouse_x, new_mouse_y;
                glfwGetCursorPos(window, &new_mouse_x, &new_mouse_y);
                double d_mouse_x = new_mouse_x - last_mouse_x, d_mouse_y = new_mouse_y - last_mouse_y;
                double delta = timer_get_elapsed(&last_frame_time);
                last_frame_time = timer_start(last_frame_time);
        	last_mouse_x = new_mouse_x; last_mouse_y = new_mouse_y;

        	// Keys
        	vec3 cam_movement = {0.0F, 0.0F, 0.0F};
        	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cam_movement[2] += MOVEMENT_SPEED;
        	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cam_movement[2] -= MOVEMENT_SPEED;
        	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cam_movement[0] -= MOVEMENT_SPEED;
        	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cam_movement[0] += MOVEMENT_SPEED;

        	// Update camera
        	camera_fly_update(&camera,
                                  d_mouse_x * MOUSE_SENSITIVITY_FACTOR, d_mouse_y * MOUSE_SENSITIVITY_FACTOR,
        	                  cam_movement, delta);

		// Set up frame
                int frame_idx = frame_ct % CONCURRENT_FRAMES;
                struct SyncSet* sync_set = &sync_sets[frame_idx];

                VkCommandBuffer cbuf = cbufs[frame_idx];

                // Wait for the render process using these sync objects to finish rendering
                res = vkWaitForFences(base.device, 1, &sync_set->render_fence, VK_TRUE, UINT64_MAX);
                assert(res == VK_SUCCESS);

                // Reset command buffer
                vkResetCommandBuffer(cbuf, 0);

                // Update camera uniform buffer
                struct CameraUniform uni_data;
                memcpy(uni_data.view, camera.view, sizeof(camera.view));
                glm_perspective(1.5F, (float) swapchain.width / (float) swapchain.height, 0.1F, 10000.0F, uni_data.proj);
                struct Buffer* ubuf = &ubufs[frame_idx];
                mem_write(base.device, ubuf->mem, sizeof(uni_data), &uni_data);

                // Acquire an image
                uint32_t image_idx;
                res = vkAcquireNextImageKHR(base.device, swapchain.handle, UINT64_MAX,
                                            sync_set->acquire_sem, VK_NULL_HANDLE, &image_idx);
                if (res == VK_ERROR_OUT_OF_DATE_KHR) {
                        must_recreate = 1;
                        continue;
                } else assert(res == VK_SUCCESS);

                // Record command buffer
                VkCommandBufferBeginInfo cbuf_begin_info = {0};
                cbuf_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                cbuf_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                vkBeginCommandBuffer(cbuf, &cbuf_begin_info);

                VkClearValue clear_vals[] = {{{{0.0F, 0.0F, 0.0F, 1.0F}}},
                                             {{{1.0F, 0.0F}}},
                                             {{{0.0F, 0.0F, 0.0F, 1.0F}}}};

                VkRenderPassBeginInfo cbuf_rpass_info = {0};
                cbuf_rpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                cbuf_rpass_info.renderPass = rpass;
                cbuf_rpass_info.framebuffer = framebuffers[image_idx];
                cbuf_rpass_info.renderArea.extent.width = swapchain.width;
                cbuf_rpass_info.renderArea.extent.height = swapchain.height;
                cbuf_rpass_info.clearValueCount = sizeof(clear_vals) / sizeof(clear_vals[0]);
                cbuf_rpass_info.pClearValues = clear_vals;
                vkCmdBeginRenderPass(cbuf, &cbuf_rpass_info, VK_SUBPASS_CONTENTS_INLINE);

                vkCmdBindPipeline(cbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

                VkDeviceSize vbuf_offset = 0;
                vkCmdBindVertexBuffers(cbuf, 0, 1, &vbuf.handle, &vbuf_offset);
                vkCmdBindIndexBuffer(cbuf, ibuf.handle, 0, VK_INDEX_TYPE_UINT32);

                VkViewport viewport = {0};
                viewport.width = swapchain.width;
                viewport.height = swapchain.height;
                viewport.minDepth = 0.0F;
                viewport.maxDepth = 1.0F;
                vkCmdSetViewport(cbuf, 0, 1, &viewport);

                VkRect2D scissor = {0};
                scissor.extent.width = swapchain.width;
                scissor.extent.height = swapchain.height;
                vkCmdSetScissor(cbuf, 0, 1, &scissor);

		// Actually draw
                vkCmdBindDescriptorSets(cbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
                                        0, 1, &sets_cam[frame_idx], 0, NULL);
                for (int i = 0; i < mesh_ct; i++) {
        		vkCmdPushConstants(cbuf, pipeline_layout,
                                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                           0, sizeof(struct PushConstants), &meshes[i].push_constants);

                        vkCmdBindDescriptorSets(cbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
                                                1, 1, &meshes[i].set, 0, NULL);
                        vkCmdDrawIndexed(cbuf, meshes[i].index_ct, 1, meshes[i].index_offset, 0, 0);
                }
                vkCmdEndRenderPass(cbuf);

                res = vkEndCommandBuffer(cbuf);
                assert(res == VK_SUCCESS);

                // Wait until whoever is rendering to the image is done
                if (image_fences[image_idx] != VK_NULL_HANDLE)
                        vkWaitForFences(base.device, 1, &image_fences[image_idx], VK_TRUE, UINT64_MAX);

                // Reset fence
                res = vkResetFences(base.device, 1, &sync_set->render_fence);
                assert(res == VK_SUCCESS);

                // Mark it as in use by us
                image_fences[image_idx] = sync_set->render_fence;

                // Submit
                VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                VkSubmitInfo submit_info = {0};
                submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submit_info.waitSemaphoreCount = 1;
                submit_info.pWaitSemaphores = &sync_set->acquire_sem;
                submit_info.pWaitDstStageMask = &wait_stage;
                submit_info.commandBufferCount = 1;
                submit_info.pCommandBuffers = &cbuf;
                submit_info.signalSemaphoreCount = 1;
                submit_info.pSignalSemaphores = &sync_set->render_sem;

                res = vkQueueSubmit(base.queue, 1, &submit_info, sync_set->render_fence);
                assert(res == VK_SUCCESS);

                // Present
                VkPresentInfoKHR present_info = {0};
                present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
                present_info.waitSemaphoreCount = 1;
                present_info.pWaitSemaphores = &sync_set->render_sem;
                present_info.swapchainCount = 1;
                present_info.pSwapchains = &swapchain.handle;
                present_info.pImageIndices = &image_idx;

                res = vkQueuePresentKHR(base.queue, &present_info);
                if (res == VK_ERROR_OUT_OF_DATE_KHR) must_recreate = 1;
                else assert(res == VK_SUCCESS);

                glfwPollEvents();
                frame_ct++;
        }

	double elapsed = timer_get_elapsed(&start_time);
        double fps = (double) frame_ct / elapsed;
        printf("FPS: %.2f\n", fps);

        // Cleanup
        vkDeviceWaitIdle(base.device);

        vkDestroyPipeline(base.device, pipeline, NULL);

        vkDestroyPipelineLayout(base.device, pipeline_layout, NULL);

        vkDestroyRenderPass(base.device, rpass, NULL);

        vkDestroyShaderModule(base.device, vs, NULL);
        vkDestroyShaderModule(base.device, fs, NULL);

        swapchain_destroy(base.device, &swapchain);

	vkDestroySampler(base.device, tex_sampler, NULL);

        buffer_destroy(base.device, &vbuf);
        buffer_destroy(base.device, &ibuf);

        for (int i = 0; i < CONCURRENT_FRAMES; i++) {
                sync_set_destroy(base.device, &sync_sets[i]);
                buffer_destroy(base.device, &ubufs[i]);
        }

        image_destroy(base.device, &depth_image);
        image_destroy(base.device, &color_image);

        for (int i = 0; i < swapchain.image_ct; i++) vkDestroyFramebuffer(base.device, framebuffers[i], NULL);

        for (int i = 0; i < mesh_ct; i++) image_destroy(base.device, &textures[i]);

        vkDestroyDescriptorPool(base.device, dpool, NULL);

	base_destroy(&base);

        glfwDestroyWindow(window);
        glfwTerminate();

	free(framebuffers);
	free(image_fences);
	free(textures);
	free(meshes);
}
