#ifndef LL_PIPELINE_H
#define LL_PIPELINE_H

#include <vulkan/vulkan.h>

#include <assert.h>

struct PipelineSettings {
        VkPipelineInputAssemblyStateCreateInfo input_assembly;
        VkPipelineRasterizationStateCreateInfo rasterizer;
        VkPipelineViewportStateCreateInfo viewport;
        VkPipelineDepthStencilStateCreateInfo depth;
        VkPipelineMultisampleStateCreateInfo multisampling;
        VkPipelineColorBlendStateCreateInfo color_blend;
        VkPipelineDynamicStateCreateInfo dynamic_state;
};

const VkPipelineColorBlendAttachmentState PIPELINE_COLOR_BLEND_ATTACHMENT_DEFAULT = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_FALSE
};

const VkDynamicState PIPELINE_DEFAULT_DYNAMIC_STATE[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

const struct PipelineSettings PIPELINE_SETTINGS_DEFAULT = {
        .input_assembly = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                .primitiveRestartEnable = VK_FALSE
        },
        .rasterizer = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .depthClampEnable = VK_FALSE,
                .rasterizerDiscardEnable = VK_FALSE,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .lineWidth = 1.0F,
                .cullMode = VK_CULL_MODE_BACK_BIT,
                .frontFace = VK_FRONT_FACE_CLOCKWISE,
                .depthBiasEnable = VK_FALSE
        },
        .viewport = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                .viewportCount = 1,
                .scissorCount = 1
        },
        .depth = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                .depthTestEnable = VK_FALSE
        },
        .multisampling = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                .sampleShadingEnable = VK_FALSE,
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
        },
        .color_blend = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .logicOpEnable = VK_FALSE,
                .attachmentCount = 1,
                .pAttachments = &PIPELINE_COLOR_BLEND_ATTACHMENT_DEFAULT
        },
        .dynamic_state = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                .dynamicStateCount = sizeof(PIPELINE_DEFAULT_DYNAMIC_STATE) / sizeof(PIPELINE_DEFAULT_DYNAMIC_STATE[0]),
                .pDynamicStates = PIPELINE_DEFAULT_DYNAMIC_STATE
        }
};

void pipeline_create(VkDevice device, const struct PipelineSettings* settings,
                     uint32_t stage_count, const VkPipelineShaderStageCreateInfo* stages,
                     const VkPipelineVertexInputStateCreateInfo* vertex_info,
                     VkPipelineLayout layout, VkRenderPass render_pass, uint32_t subpass,
                     VkPipeline* pipeline)
{
        VkGraphicsPipelineCreateInfo info = {0};
        info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        info.stageCount = stage_count;
        info.pStages = stages;
        info.pVertexInputState = vertex_info;
        info.pInputAssemblyState = &settings->input_assembly;
        info.pRasterizationState = &settings->rasterizer;
        info.pViewportState = &settings->viewport;
        info.pDepthStencilState = &settings->depth;
        info.pMultisampleState = &settings->multisampling;
        info.pColorBlendState = &settings->color_blend;
        info.pDynamicState = &settings->dynamic_state;
        info.layout = layout;
        info.renderPass = render_pass;
        info.subpass = subpass;

        VkResult res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, NULL, pipeline);
        assert(res == VK_SUCCESS);
}

#endif // LL_PIPELINE_H

