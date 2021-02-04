#ifndef LL_RPASS_H
#define LL_RPASS_H

#include <vulkan/vulkan.h>

#include <assert.h>

void rpass_basic(VkDevice device, VkFormat format, VkRenderPass* rpass) {
        VkAttachmentDescription attach_color = {0};
        attach_color.format = format;
        attach_color.samples = VK_SAMPLE_COUNT_1_BIT;
        attach_color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attach_color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attach_color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attach_color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attach_color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attach_color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference ref_color = {0};
        ref_color.attachment = 0;
        ref_color.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {0};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &ref_color;

        VkSubpassDependency subpass_dep = {0};
        subpass_dep.srcSubpass = VK_SUBPASS_EXTERNAL;
        subpass_dep.dstSubpass = 0;
        subpass_dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpass_dep.srcAccessMask = 0;
        subpass_dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpass_dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo info = {0};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = 1;
        info.pAttachments = &attach_color;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 1;
        info.pDependencies = &subpass_dep;

        VkResult res = vkCreateRenderPass(device, &info, NULL, rpass);
        assert(res == VK_SUCCESS);
}

#endif // LL_RPASS_H
