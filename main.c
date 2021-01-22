#include "external/cglm/include/cglm/cglm.h"
#define STBI_ONLY_TGA
#define STBI_ONLY_PNG
#define STBI_FAILURE_USERMSG
#define STB_IMAGE_IMPLEMENTATION
#include "external/stb_image/stb_image.h"
#define FAST_OBJ_IMPLEMENTATION
#include "external/fast_obj/fast_obj.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include "src/ll/base.h"
#include "src/ll/buffer.h"
#include "src/ll/image.h"
#include "src/ll/pipeline.h"
#include "src/ll/set.h"
#include "src/ll/shader.h"
#include "src/ll/swapchain.h"
#include "src/ll/sync.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

const int INSTANCE_EXT_CT = 1;
const char* INSTANCE_EXTS[] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};

const char* DEVICE_EXTS[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

const int DEVICE_EXT_CT = 1;
const VkFormat SC_FORMAT_PREF = VK_FORMAT_B8G8R8A8_SRGB;
const VkPresentModeKHR SC_PRESENT_MODE_PREF = VK_PRESENT_MODE_IMMEDIATE_KHR;
const VkFormat DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;

#define CONCURRENT_FRAMES 4

#ifndef NDEBUG
        const int VALIDATION_ON = 1;
#else
	const int VALIDATION_ON = 0;
#endif

struct Vertex {
        vec3 pos;
        vec2 tex_c;
};

struct Uniform {
        mat4 model;
        mat4 view;
        mat4 proj;
};

struct SyncSet {
        VkFence render_fence;
        VkSemaphore acquire_sem;
        VkSemaphore render_sem;
};

struct Object {
        struct Buffer vertices;
        uint32_t vertex_ct;
        struct Buffer indices;
        uint32_t index_ct;
        VkDescriptorSet set;
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

// Allocates own memory, call stbi_image_free afterwards
stbi_uc* load_tex(const char* path, int* width, int* height) {
	int channels;
        return stbi_load(path, width, height, &channels, STBI_rgb_alpha);
}

void depth_create(VkPhysicalDevice phys_dev, VkDevice device,
                  VkFormat format, uint32_t width, uint32_t height, struct Image* image)
{
	image_create(phys_dev, device, format, width, height,
	             VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	             VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
	             VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, image);
}

int main() {
        // GLFW
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan", NULL, NULL);

        // Base
        struct Base base;
        base_create(window, VALIDATION_ON, INSTANCE_EXT_CT, INSTANCE_EXTS, DEVICE_EXT_CT, DEVICE_EXTS, &base);

        // Load model
        fastObjMesh* mesh = fast_obj_read("assets/models/dragon.obj");

	uint32_t vertex_ct = mesh->face_count * 3;
	struct Vertex* vertices = malloc(vertex_ct * sizeof(vertices[0]));
	uint32_t vertex_idx = 0;

	uint32_t index_ct = mesh->face_count * 3;
	uint32_t* indices = malloc(index_ct * sizeof(indices[0]));
	for (int i = 0; i < mesh->face_count; i++) {
        	for (int j = 0; j < 3; j++) {
                	uint32_t v_idx = mesh->indices[3*i+j].p;
                	uint32_t n_idx = mesh->indices[3*i+j].n;
                	indices[3*i+j] = v_idx;
                	vertices[v_idx].pos[0] = mesh->positions[3*v_idx+0];
                	vertices[v_idx].pos[1] = mesh->positions[3*v_idx+1];
                	vertices[v_idx].pos[2] = mesh->positions[3*v_idx+2];
                	vertices[v_idx].tex_c[0] = mesh->normals[3*n_idx+0];
                	vertices[v_idx].tex_c[1] = mesh->normals[3*n_idx+1];
                	vertex_idx++;
        	}
	}

	fast_obj_destroy(mesh);

        // Meshes
        struct Buffer vbuf, ibuf;
        buffer_staged(base.phys_dev, base.device, base.queue, base.cpool, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertex_ct * sizeof(vertices[0]), vertices, &vbuf);
        buffer_staged(base.phys_dev, base.device, base.queue, base.cpool, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, index_ct * sizeof(indices[0]), indices, &ibuf);

        free(vertices);
        free(indices);

        // Load textures
        int tex_width, tex_height;
	stbi_uc* pixels_diffuse = load_tex("assets/cat.png",
                                           &tex_width, &tex_height);
	assert(pixels_diffuse != NULL);
	VkDeviceSize tex_diffuse_size = tex_width * tex_height * 4;

        int tex_normal_width, tex_normal_height;
	stbi_uc* pixels_normal = load_tex("assets/cat.png",
                                           &tex_normal_width, &tex_normal_height);
	assert(pixels_normal != NULL);
	assert(tex_normal_width == tex_width && tex_normal_height == tex_height);

	// Create source buffer
	struct Buffer tex_buf;
	buffer_create(base.phys_dev, base.device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	              tex_diffuse_size, &tex_buf);
	mem_write(base.device, tex_buf.mem, tex_diffuse_size, pixels_diffuse);
	stbi_image_free(pixels_diffuse);

	// Create diffuse texture
	struct Image tex_diffuse;
	image_create(base.phys_dev, base.device, VK_FORMAT_R8G8B8A8_SRGB, tex_width, tex_height,
	             VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		     VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT, &tex_diffuse);

	// Copy to diffuse texture
	image_trans(base.device, base.queue, base.cpool, tex_diffuse.handle, VK_IMAGE_ASPECT_COLOR_BIT,
	            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	            0, VK_ACCESS_TRANSFER_WRITE_BIT,
	            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	image_copy_from_buffer(base.device, base.queue, base.cpool, VK_IMAGE_ASPECT_COLOR_BIT,
	                       tex_buf.handle, tex_diffuse.handle, tex_width, tex_height);

	image_trans(base.device, base.queue, base.cpool, tex_diffuse.handle, VK_IMAGE_ASPECT_COLOR_BIT,
	            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
	            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

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

	VkSampler tex_sampler;
	VkResult res = vkCreateSampler(base.device, &sampler_info, NULL, &tex_sampler);
	assert(res == VK_SUCCESS);

        // Swapchain
        struct Swapchain swapchain;
        swapchain_create(base.surface, base.phys_dev, base.device, SC_FORMAT_PREF, SC_PRESENT_MODE_PREF, &swapchain);

        // Vertex input
        VkVertexInputBindingDescription vtx_bind_desc = {0};
        vtx_bind_desc.binding = 0;
        vtx_bind_desc.stride = sizeof(struct Vertex);
        vtx_bind_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription vtx_attr_descs[2] = {0};
        vtx_attr_descs[0].binding = 0;
        vtx_attr_descs[0].location = 0;
        vtx_attr_descs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        vtx_attr_descs[0].offset = offsetof(struct Vertex, pos);
        vtx_attr_descs[1].binding = 0;
        vtx_attr_descs[1].location = 1;
        vtx_attr_descs[1].format = VK_FORMAT_R32G32_SFLOAT;
        vtx_attr_descs[1].offset = offsetof(struct Vertex, tex_c);

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
        VkAttachmentDescription attachments[2] = {0};
        attachments[0].format = swapchain.format;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        attachments[1].format = DEPTH_FORMAT;
        attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_ref = {0};
        color_ref.attachment = 0;
        color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depth_ref = {0};
        depth_ref.attachment = 1;
        depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {0};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_ref;
        subpass.pDepthStencilAttachment = &depth_ref;

        VkSubpassDependency subpass_dep = {0};
        subpass_dep.srcSubpass = VK_SUBPASS_EXTERNAL;
        subpass_dep.dstSubpass = 0;
        subpass_dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpass_dep.srcAccessMask = 0;
        subpass_dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpass_dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

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

        // Descriptor set layouts
	VkDescriptorSetLayout set_layouts[2];

       	VkDescriptorSetLayoutBinding set_cam_desc = {0};
        set_cam_desc.binding = 0;
        set_cam_desc.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        set_cam_desc.descriptorCount = 1;
        set_cam_desc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        set_layout_create(base.device, 1, &set_cam_desc, &set_layouts[0]);

	VkDescriptorSetLayoutBinding set_tex_desc = {0};
        set_tex_desc.binding = 0;
        set_tex_desc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        set_tex_desc.descriptorCount = 1;
        set_tex_desc.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        set_layout_create(base.device, 1, &set_tex_desc, &set_layouts[1]);

        // Pipeline layout
        VkPipelineLayoutCreateInfo pipeline_lt_info = {0};
        pipeline_lt_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_lt_info.setLayoutCount = sizeof(set_layouts) / sizeof(set_layouts[0]);
        pipeline_lt_info.pSetLayouts = set_layouts;

        VkPipelineLayout pipeline_lt;
        res = vkCreatePipelineLayout(base.device, &pipeline_lt_info, NULL, &pipeline_lt);
        assert(res == VK_SUCCESS);

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

	VkPipeline pipeline;
	pipeline_create(base.device, &pipeline_settings,
	                sizeof(shaders) / sizeof(shaders[0]), shaders, &vertex_info,
	                pipeline_lt, rpass, 0, &pipeline);

	// Depth buffers
	struct Image* depth_images = malloc(swapchain.image_ct * sizeof(depth_images[0]));
	for (int i = 0; i < swapchain.image_ct; ++i) {
        	depth_create(base.phys_dev, base.device, DEPTH_FORMAT, swapchain.width, swapchain.height, &depth_images[i]);
	};

        // Framebuffers
        VkFramebuffer* framebuffers = malloc(swapchain.image_ct * sizeof(framebuffers[0]));
        for (int i = 0; i < swapchain.image_ct; ++i) {
                VkImageView views[] = {swapchain.views[i], depth_images[i].view};
                framebuffer_create(base.device, rpass, swapchain.width, swapchain.height,
                                   sizeof(views) / sizeof(views[0]), views, &framebuffers[i]);
        }

        // Command buffers
        VkCommandBuffer cbufs[CONCURRENT_FRAMES];
        for (int i = 0; i < CONCURRENT_FRAMES; ++i) cbuf_alloc(base.device, base.cpool, &cbufs[i]);

        // Sync sets
        struct SyncSet sync_sets [CONCURRENT_FRAMES];
        for (int i = 0; i < CONCURRENT_FRAMES; ++i) sync_set_create(base.device, &sync_sets[i]);

        // Uniform buffers (one for every render process)
        struct Buffer ubufs[CONCURRENT_FRAMES];
        for (int i = 0; i < CONCURRENT_FRAMES; ++i) {
                buffer_create(base.phys_dev, base.device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                              sizeof(struct Uniform), &ubufs[i]);
        };

        // Descriptor pool
        VkDescriptorPoolSize dpool_sizes[2] = {0};
        dpool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        dpool_sizes[0].descriptorCount = CONCURRENT_FRAMES;
        dpool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        dpool_sizes[1].descriptorCount = CONCURRENT_FRAMES;

        VkDescriptorPoolCreateInfo dpool_info = {0};
        dpool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        dpool_info.poolSizeCount = sizeof(dpool_sizes) / sizeof(dpool_sizes[0]);
        dpool_info.pPoolSizes = dpool_sizes;
        dpool_info.maxSets = 2 * CONCURRENT_FRAMES;

        VkDescriptorPool dpool;
        res = vkCreateDescriptorPool(base.device, &dpool_info, NULL, &dpool);
        assert(res == VK_SUCCESS);

        // Sets
        VkDescriptorSet sets_cam[CONCURRENT_FRAMES];
        VkDescriptorSet sets_tex[CONCURRENT_FRAMES];
        for (int i = 0; i < CONCURRENT_FRAMES; ++i) {
                struct Descriptor desc_cam = {0};
                desc_cam.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                desc_cam.binding = 0;
                desc_cam.shader_stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
                desc_cam.buffer.buffer = ubufs[i].handle;
                desc_cam.buffer.range = ubufs[i].size;
                set_create(base.device, dpool, set_layouts[0],
                           1, &desc_cam, &sets_cam[i]);

                struct Descriptor desc_tex = {0};
                desc_tex.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                desc_tex.binding = 0;
                desc_tex.shader_stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
                desc_tex.image.sampler = tex_sampler;
                desc_tex.image.imageView = tex_diffuse.view;
                desc_tex.image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                set_create(base.device, dpool, set_layouts[1],
                           1, &desc_tex, &sets_tex[i]);
        }

	for (int i = 0; i < sizeof(set_layouts) / sizeof(set_layouts[0]); ++i) {
                vkDestroyDescriptorSetLayout(base.device, set_layouts[i], NULL);
	}

        // Image fences
        VkFence* image_fences = malloc(swapchain.image_ct * sizeof(image_fences[0]));
        for (int i = 0; i < swapchain.image_ct; ++i) image_fences[i] = VK_NULL_HANDLE;

        // Main loop
        int frame_ct = 0;
        struct timespec start_time;
        clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);
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

                        for (int i = 0; i < CONCURRENT_FRAMES; ++i) {
                                sync_set_destroy(base.device, &sync_sets[i]);
                                sync_set_create(base.device, &sync_sets[i]);
                        }

                        for (int i = 0; i < swapchain.image_ct; ++i) {
                                vkDestroyFramebuffer(base.device, framebuffers[i], NULL);

                                image_destroy(base.device, &depth_images[i]);
                                depth_create(base.phys_dev, base.device, DEPTH_FORMAT,
                                             swapchain.width, swapchain.height, &depth_images[i]);

                                VkImageView new_framebuffer_views[] = {swapchain.views[i], depth_images[i].view};
                                framebuffer_create(base.device, rpass, swapchain.width, swapchain.height,
                                                   sizeof(new_framebuffer_views) / sizeof(new_framebuffer_views[0]),
                                                   new_framebuffer_views, &framebuffers[i]);

                                image_fences[i] = VK_NULL_HANDLE;
                        }

                        int real_width, real_height;
                        glfwGetFramebufferSize(window, &real_width, &real_height);
                        if (real_width != swapchain.width || real_height != swapchain.height) must_recreate = 1;
                }

                int frame_idx = frame_ct % CONCURRENT_FRAMES;
                struct SyncSet* sync_set = &sync_sets[frame_idx];

                VkCommandBuffer cbuf = cbufs[frame_idx];

                // Write to ubuf
                struct timespec time;
                clock_gettime(CLOCK_MONOTONIC_RAW, &time);
                double elapsed = (double) ((time.tv_sec * 1000000000 + time.tv_nsec)
                                         - (start_time.tv_sec * 1000000000 + start_time.tv_nsec)) / 1000000000.0F;
                vec3 eye = {2.0F * sin(elapsed * 0.2), 0.5F, 2.0F * cos(elapsed * 0.2)};
                vec3 looking_at = {0.0F, 0.0F, 0.0F};

                struct Uniform uni_data;
                glm_mat4_identity(uni_data.model);
                glm_lookat(eye, looking_at, (vec3){0.0F, -1.0F, 0.0F}, uni_data.view);
                glm_perspective(1.5F, (float) swapchain.width / (float) swapchain.height, 0.1F, 10000.0F, uni_data.proj);
                struct Buffer* ubuf = &ubufs[frame_idx];
                mem_write(base.device, ubuf->mem, sizeof(uni_data), &uni_data);

                // Wait for the render process using these sync objects to finish rendering
                res = vkWaitForFences(base.device, 1, &sync_set->render_fence, VK_TRUE, UINT64_MAX);
                assert(res == VK_SUCCESS);

                // Reset command buffer
                vkResetCommandBuffer(cbuf, 0);

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

                VkClearValue clear_vals[2] = {{{{0.0F, 0.0F, 0.0F, 1.0F}}},
                                              {{{1.0F, 0.0F}}}};

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

                vkCmdBindDescriptorSets(cbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_lt,
                                        0, 1, &sets_cam[frame_idx], 0, NULL);
                vkCmdBindDescriptorSets(cbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_lt,
                                        1, 1, &sets_tex[frame_idx], 0, NULL);
                vkCmdDrawIndexed(cbuf, index_ct, 1, 0, 0, 0);
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
                ++frame_ct;
        }

        struct timespec stop_time;
        clock_gettime(CLOCK_MONOTONIC_RAW, &stop_time);
        double elapsed = (double) ((stop_time.tv_sec * 1000000000 + stop_time.tv_nsec)
                                 - (start_time.tv_sec * 1000000000 + start_time.tv_nsec)) / 1000000000.0F;
        double fps = (double) frame_ct / elapsed;
        printf("FPS: %.2f\n", fps);

        // Cleanup
        vkDeviceWaitIdle(base.device);

        vkDestroyPipeline(base.device, pipeline, NULL);

        vkDestroyPipelineLayout(base.device, pipeline_lt, NULL);

        vkDestroyRenderPass(base.device, rpass, NULL);

        vkDestroyShaderModule(base.device, vs, NULL);
        vkDestroyShaderModule(base.device, fs, NULL);

        swapchain_destroy(base.device, &swapchain);

	image_destroy(base.device, &tex_diffuse);
	vkDestroySampler(base.device, tex_sampler, NULL);
        buffer_destroy(base.device, &tex_buf);

        buffer_destroy(base.device, &vbuf);
        buffer_destroy(base.device, &ibuf);

        for (int i = 0; i < CONCURRENT_FRAMES; ++i) {
                sync_set_destroy(base.device, &sync_sets[i]);
                buffer_destroy(base.device, &ubufs[i]);
        }

        for (int i = 0; i < swapchain.image_ct; ++i) {
                vkDestroyFramebuffer(base.device, framebuffers[i], NULL);
                image_destroy(base.device, &depth_images[i]);
        }

        vkDestroyDescriptorPool(base.device, dpool, NULL);

	base_destroy(&base);

        glfwDestroyWindow(window);
        glfwTerminate();

	free(framebuffers);
	free(image_fences);
	free(depth_images);
}
