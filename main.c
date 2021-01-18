#include <cglm/cglm.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include "src/ll/base.h"
#include "src/ll/buffer.h"
#include "src/ll/render_proc.h"
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

const int CONCURRENT_FRAMES_MAX = 4;

#ifndef NDEBUG
        const int VALIDATION_ON = 1;
#else
	const int VALIDATION_ON = 0;
#endif

struct Vertex {
        vec2 pos;
        vec3 color;
};

const struct Vertex VERTICES[] = {{{-0.8F, -0.8F}, {1.0F, 0.0F, 0.0F}},
                                   {{0.8F, -0.8F}, {0.0F, 1.0F, 0.0F}},
                                   {{-0.8F, 0.8F}, {0.0F, 0.0F, 1.0F}},
                                   {{0.8F, 0.8F}, {1.0F, 0.0F, 1.0F}}};
const uint32_t VERTEX_CT = sizeof(VERTICES) / sizeof(VERTICES[0]);
const uint16_t INDICES[] = {0, 1, 3, 0, 3, 2};
const uint32_t INDEX_CT = sizeof(INDICES) / sizeof(INDICES[0]);

struct Uniform {
        mat4 model;
        mat4 view;
        mat4 proj;
};

// Memory must be preallocated
void fbs_create(VkDevice device, VkRenderPass rpass, uint32_t width, uint32_t height,
                uint32_t view_ct, VkImageView* views, VkFramebuffer* fbs)
{
        for (int i = 0; i < view_ct; ++i) {
                VkFramebufferCreateInfo info = {0};
                info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                info.renderPass = rpass;
                info.attachmentCount = 1;
                info.pAttachments = &views[i];
                info.width = width;
                info.height = height;
                info.layers = 1;

                VkResult res = vkCreateFramebuffer(device, &info, NULL, &fbs[i]);
                assert(res == VK_SUCCESS);
        }
}

// Does not free fbs
void fbs_destroy(VkDevice device, uint32_t fb_ct, VkFramebuffer* fbs) {
        for (int i = 0; i < fb_ct; ++i) vkDestroyFramebuffer(device, fbs[i], NULL);
}

int main() {
        // GLFW
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan", NULL, NULL);

        // Base
        struct Base base;
        base_create(window, VALIDATION_ON, INSTANCE_EXT_CT, INSTANCE_EXTS, DEVICE_EXT_CT, DEVICE_EXTS, &base);

        // Command pool
        VkCommandPoolCreateInfo cpool_info = {0};
        cpool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cpool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cpool_info.queueFamilyIndex = base.queue_fam;

        VkCommandPool cpool;
        VkResult res = vkCreateCommandPool(base.device, &cpool_info, NULL, &cpool);
        assert(res == VK_SUCCESS);

        // Meshes
        struct Buffer vbuf, ibuf;
        buffer_staged(base.phys_dev, base.device, base.queue, cpool, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, sizeof(VERTICES), VERTICES, &vbuf);
        buffer_staged(base.phys_dev, base.device, base.queue, cpool, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, sizeof(INDICES), INDICES, &ibuf);

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
        vtx_attr_descs[0].format = VK_FORMAT_R32G32_SFLOAT;
        vtx_attr_descs[0].offset = offsetof(struct Vertex, pos);
        vtx_attr_descs[1].binding = 0;
        vtx_attr_descs[1].location = 1;
        vtx_attr_descs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        vtx_attr_descs[1].offset = offsetof(struct Vertex, color);

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
        VkAttachmentDescription attach_desc = {0};
        attach_desc.format = swapchain.format;
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
        res = vkCreateRenderPass(base.device, &rpass_info, NULL, &rpass);
        assert(res == VK_SUCCESS);

        // Descriptor set layout
        VkDescriptorSetLayoutBinding desc_lt_bind = {0};
        desc_lt_bind.binding = 0;
        desc_lt_bind.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc_lt_bind.descriptorCount = 1;
        desc_lt_bind.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo desc_lt_info = {0};
        desc_lt_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        desc_lt_info.bindingCount = 1;
        desc_lt_info.pBindings = &desc_lt_bind;

        VkDescriptorSetLayout desc_lt = {0};
        res = vkCreateDescriptorSetLayout(base.device, &desc_lt_info, NULL, &desc_lt);
        assert(res == VK_SUCCESS);

        // Pipeline layout
        VkPipelineLayoutCreateInfo pipeline_lt_info = {0};
        pipeline_lt_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_lt_info.setLayoutCount = 1;
        pipeline_lt_info.pSetLayouts = &desc_lt;

        VkPipelineLayout pipeline_lt;
        res = vkCreatePipelineLayout(base.device, &pipeline_lt_info, NULL, &pipeline_lt);
        assert(res == VK_SUCCESS);

        // Pipeline
        VkPipelineVertexInputStateCreateInfo input_info = {0};
        input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        input_info.vertexBindingDescriptionCount = 1;
        input_info.pVertexBindingDescriptions = &vtx_bind_desc;
        input_info.vertexAttributeDescriptionCount = 2;
        input_info.pVertexAttributeDescriptions = vtx_attr_descs;

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
        rasterizer.cullMode = VK_CULL_MODE_NONE;
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
        res = vkCreateGraphicsPipelines(base.device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline);
        assert(res == VK_SUCCESS);

        // Framebuffers
        VkFramebuffer* fbs = malloc(swapchain.image_ct * sizeof(fbs[0]));
        fbs_create(base.device, rpass, swapchain.width, swapchain.height, swapchain.image_ct, swapchain.views, fbs);

        // Render processes
        uint32_t rproc_ct = CONCURRENT_FRAMES_MAX;
        struct RenderProc* rprocs = malloc(rproc_ct * sizeof(rprocs[0]));
        for (int i = 0; i < rproc_ct; ++i) render_proc_create(base.device, cpool, &rprocs[i]);

        // Uniform buffers (one for every render process)
        struct Buffer* ubufs = malloc(rproc_ct * sizeof(ubufs[0]));
        for (int i = 0; i < rproc_ct; ++i) {
                buffer_create(base.phys_dev, base.device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                              sizeof(struct Uniform), &ubufs[i]);
        };

        // Descriptor pool
        VkDescriptorPoolSize dpool_size = {0};
        dpool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        dpool_size.descriptorCount = rproc_ct;

        VkDescriptorPoolCreateInfo dpool_info = {0};
        dpool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        dpool_info.poolSizeCount = 1;
        dpool_info.pPoolSizes = &dpool_size;
        dpool_info.maxSets = rproc_ct;

        VkDescriptorPool dpool;
        res = vkCreateDescriptorPool(base.device, &dpool_info, NULL, &dpool);
        assert(res == VK_SUCCESS);

        // Descriptor sets
        VkDescriptorSetLayout* set_layouts = malloc(rproc_ct * sizeof(set_layouts[0]));
        for (int i = 0; i < rproc_ct; ++i) set_layouts[i] = desc_lt;

        VkDescriptorSetAllocateInfo set_info = {0};
        set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        set_info.descriptorPool = dpool;
        set_info.descriptorSetCount = rproc_ct;
        set_info.pSetLayouts = set_layouts;

        VkDescriptorSet* sets = malloc(rproc_ct * sizeof(sets[0]));
        res = vkAllocateDescriptorSets(base.device, &set_info, sets);
        assert(res == VK_SUCCESS);

        free(set_layouts);

        for (int i = 0; i < rproc_ct; ++i) {
                VkDescriptorBufferInfo desc_buf = {0};
                desc_buf.buffer = ubufs[i].handle;
                desc_buf.range = sizeof(ubufs[i]);

                VkWriteDescriptorSet desc_write = {0};
                desc_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                desc_write.dstSet = sets[i];
                desc_write.dstBinding = 0;
                desc_write.dstArrayElement = 0;
                desc_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                desc_write.descriptorCount = 1;
                desc_write.pBufferInfo = &desc_buf;

                vkUpdateDescriptorSets(base.device, 1, &desc_write, 0, NULL);
        }

        vkDestroyDescriptorSetLayout(base.device, desc_lt, NULL);

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

                        fbs_destroy(base.device, swapchain.image_ct, fbs);

                        swapchain_destroy(base.device, &swapchain);
                        swapchain_create(base.surface, base.phys_dev, base.device,
                                         SC_FORMAT_PREF, SC_PRESENT_MODE_PREF, &swapchain);

                        assert(swapchain.format == old_format && swapchain.image_ct == old_image_ct);

                        fbs_create(base.device, rpass, swapchain.width, swapchain.height,
                                   swapchain.image_ct, swapchain.views, fbs);

                        for (int i = 0; i < CONCURRENT_FRAMES_MAX; ++i) {
                                render_proc_destroy_sync(base.device, &rprocs[i]);
                                render_proc_create_sync(base.device, &rprocs[i]);
                        }

                        for (int i = 0; i < swapchain.image_ct; ++i) image_fences[i] = VK_NULL_HANDLE;

                        int real_width, real_height;
                        glfwGetFramebufferSize(window, &real_width, &real_height);
                        if (real_width != swapchain.width || real_height != swapchain.height) must_recreate = 1;
                }

                int rproc_idx = frame_ct % rproc_ct;
                struct RenderProc* const rproc = &rprocs[rproc_idx];

                // Write to ubuf
                struct timespec time;
                clock_gettime(CLOCK_MONOTONIC_RAW, &time);
                double elapsed = (double) ((time.tv_sec * 1000000000 + time.tv_nsec)
                                         - (start_time.tv_sec * 1000000000 + start_time.tv_nsec)) / 1000000000.0F;
                vec3 eye = {5.0F * sin(elapsed), 2.0F, 5.0F * cos(elapsed)};

                struct Uniform uni_data;
                glm_mat4_identity(uni_data.model);
                glm_lookat(eye, (vec3){0.0F, 0.0F, 0.0F}, (vec3){0.0F, -1.0F, 0.0F}, uni_data.view);
                glm_perspective(1.0F, 1.0F, 0.1F, 10.0F, uni_data.proj);
                struct Buffer* ubuf = &ubufs[rproc_idx];
                buffer_mem_write(base.device, ubuf->mem, sizeof(uni_data), &uni_data);

                // Wait for the render process using these sync objects to finish rendering
                res = vkWaitForFences(base.device, 1, &rproc->fence, VK_TRUE, UINT64_MAX);
                assert(res == VK_SUCCESS);

                // Reset command buffer
                vkResetCommandBuffer(rproc->cbuf, 0);

                // Acquire an image
                uint32_t image_idx;
                res = vkAcquireNextImageKHR(base.device, swapchain.handle, UINT64_MAX,
                                            rproc->acquire_sem, VK_NULL_HANDLE, &image_idx);
                if (res == VK_ERROR_OUT_OF_DATE_KHR) {
                        must_recreate = 1;
                        continue;
                } else assert(res == VK_SUCCESS);

                // Record command buffer
                VkCommandBufferBeginInfo cbuf_begin_info = {0};
                cbuf_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                cbuf_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                vkBeginCommandBuffer(rproc->cbuf, &cbuf_begin_info);

                VkClearValue clear_color = {{{0.0F, 0.0F, 0.0F, 1.0F}}};

                VkRenderPassBeginInfo cbuf_rpass_info = {0};
                cbuf_rpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                cbuf_rpass_info.renderPass = rpass;
                cbuf_rpass_info.framebuffer = fbs[image_idx];
                cbuf_rpass_info.renderArea.extent.width = swapchain.width;
                cbuf_rpass_info.renderArea.extent.height = swapchain.height;
                cbuf_rpass_info.clearValueCount = 1;
                cbuf_rpass_info.pClearValues = &clear_color;
                vkCmdBeginRenderPass(rproc->cbuf, &cbuf_rpass_info, VK_SUBPASS_CONTENTS_INLINE);

                vkCmdBindPipeline(rproc->cbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

                VkDeviceSize vbuf_offset = 0;
                vkCmdBindVertexBuffers(rproc->cbuf, 0, 1, &vbuf.handle, &vbuf_offset);
                vkCmdBindIndexBuffer(rproc->cbuf, ibuf.handle, 0, VK_INDEX_TYPE_UINT16);

                VkViewport viewport = {0};
                viewport.width = swapchain.width;
                viewport.height = swapchain.height;
                viewport.minDepth = 0.0F;
                viewport.maxDepth = 1.0F;
                vkCmdSetViewport(rproc->cbuf, 0, 1, &viewport);

                VkRect2D scissor = {0};
                scissor.extent.width = swapchain.width;
                scissor.extent.height = swapchain.height;
                vkCmdSetScissor(rproc->cbuf, 0, 1, &scissor);

                vkCmdBindDescriptorSets(rproc->cbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_lt,
                                        0, 1, &sets[rproc_idx], 0, NULL);
                vkCmdDrawIndexed(rproc->cbuf, INDEX_CT, 1, 0, 0, 0);
                vkCmdEndRenderPass(rproc->cbuf);

                res = vkEndCommandBuffer(rproc->cbuf);
                assert(res == VK_SUCCESS);

                // Wait until whoever is rendering to the image is done
                if (image_fences[image_idx] != VK_NULL_HANDLE)
                        vkWaitForFences(base.device, 1, &image_fences[image_idx], VK_TRUE, UINT64_MAX);

                // Reset fence
                res = vkResetFences(base.device, 1, &rproc->fence);
                assert(res == VK_SUCCESS);

                // Mark it as in use by us
                image_fences[image_idx] = rproc->fence;

                // Submit
                VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                VkSubmitInfo submit_info = {0};
                submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submit_info.waitSemaphoreCount = 1;
                submit_info.pWaitSemaphores = &rproc->acquire_sem;
                submit_info.pWaitDstStageMask = &wait_stage;
                submit_info.commandBufferCount = 1;
                submit_info.pCommandBuffers = &rproc->cbuf;
                submit_info.signalSemaphoreCount = 1;
                submit_info.pSignalSemaphores = &rproc->render_sem;

                res = vkQueueSubmit(base.queue, 1, &submit_info, rproc->fence);
                assert(res == VK_SUCCESS);

                // Present
                VkPresentInfoKHR present_info = {0};
                present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
                present_info.waitSemaphoreCount = 1;
                present_info.pWaitSemaphores = &rproc->render_sem;
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

        for (int i = 0; i < rproc_ct; ++i) render_proc_destroy_sync(base.device, &rprocs[i]);

        vkDestroyPipeline(base.device, pipeline, NULL);

        vkDestroyPipelineLayout(base.device, pipeline_lt, NULL);

        vkDestroyCommandPool(base.device, cpool, NULL);
        vkDestroyRenderPass(base.device, rpass, NULL);

        vkDestroyShaderModule(base.device, vs, NULL);
        vkDestroyShaderModule(base.device, fs, NULL);

        fbs_destroy(base.device, swapchain.image_ct, fbs);
        swapchain_destroy(base.device, &swapchain);

        buffer_destroy(base.device, &vbuf);
        buffer_destroy(base.device, &ibuf);

        for (int i = 0; i < rproc_ct; ++i) buffer_destroy(base.device, &ubufs[i]);

        vkDestroyDescriptorPool(base.device, dpool, NULL);

	base_destroy(&base);

        glfwDestroyWindow(window);
        glfwTerminate();
}
