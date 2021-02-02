#ifndef LL_RPASS_H
#define LL_RPASS_H

#include <vulkan/vulkan.h>

#include <assert.h>

#define RPASS_MAX_ATTACHMENTS 256

// The attachment references are automatically created in the render pass creationn helper.
// [layout] defines the layout for this reference, which is not necessarily the same as
// the attachment's initialLayout or finalLayout.
struct RpassAttachment {
        VkAttachmentDescription desc;
        VkImageLayout layout;
};

struct RpassSubpass {
        VkPipelineBindPoint bind_point;

        uint32_t input_attach_ct;
        struct RpassAttachment* input_attachs;
        uint32_t color_attach_ct;
        struct RpassAttachment* color_attachs;
        // Either NULL or [color_attach_ct] attachments
        struct RpassAttachment* resolve_attachs;

        struct RpassAttachment* depth_attach;
};

const struct RpassAttachment RPASS_DEFAULT_ATTACH_COLOR = {
        .desc = {
                // Format should be overridden by user
                .format = VK_FORMAT_UNDEFINED,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        },
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
};

VkSubpassDependency RPASS_DEFAULT_DEPENDENCY_COLOR_TO_EXTERNAL = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
};

void rpass_create(VkDevice device,
                  uint32_t subpass_ct, const struct RpassSubpass* subpasses,
                  uint32_t dependency_ct, const VkSubpassDependency* dependencies,
                  VkRenderPass* rpass)
{
        // Flatten attachment descriptions, create references and create subpasses
        uint32_t attach_ct = 0;
        VkAttachmentDescription attach_descs[RPASS_MAX_ATTACHMENTS] = {0};
        VkAttachmentReference attach_refs[RPASS_MAX_ATTACHMENTS] = {0};

	assert(subpass_ct < RPASS_MAX_ATTACHMENTS);
        VkSubpassDescription subpass_descs[RPASS_MAX_ATTACHMENTS] = {0};

	for (int i = 0; i < subpass_ct; i++) {
        	const struct RpassSubpass* subpass = &subpasses[i];

		subpass_descs[i].pipelineBindPoint = subpasses[i].bind_point;

		subpass_descs[i].inputAttachmentCount = subpass->input_attach_ct;
		subpass_descs[i].pInputAttachments = attach_refs + attach_ct;
        	for (int j = 0; j < subpass->input_attach_ct; j++) {
                	const struct RpassAttachment* subpass_attach = &subpass->input_attachs[j];
                	attach_descs[attach_ct] = subpass_attach->desc;
                	attach_refs[attach_ct].attachment = attach_ct;
                	attach_refs[attach_ct].layout = subpass_attach->layout;
                	attach_ct++;
        	}

		subpass_descs[i].colorAttachmentCount = subpass->color_attach_ct;
		subpass_descs[i].pColorAttachments = attach_refs + attach_ct;
        	for (int j = 0; j < subpass->color_attach_ct; j++) {
                	const struct RpassAttachment* subpass_attach = &subpass->color_attachs[j];
                	attach_descs[attach_ct] = subpass_attach->desc;
                	attach_refs[attach_ct].attachment = attach_ct;
                	attach_refs[attach_ct].layout = subpass_attach->layout;
                	attach_ct++;
        	}

		if (subpasses[i].resolve_attachs != NULL) {
        		subpass_descs[i].pResolveAttachments = attach_refs + attach_ct;
                	for (int j = 0; j < subpass->color_attach_ct; j++) {
                        	const struct RpassAttachment* subpass_attach = &subpass->resolve_attachs[j];
                        	attach_descs[attach_ct] = subpass_attach->desc;
                        	attach_refs[attach_ct].attachment = attach_ct;
                        	attach_refs[attach_ct].layout = subpass_attach->layout;
                        	attach_ct++;
                	}
		}

        	if (subpass->depth_attach != NULL) {
                	attach_descs[attach_ct] = subpass->depth_attach->desc;
                	attach_refs[attach_ct].attachment = attach_ct;
                	attach_refs[attach_ct].layout = subpass->depth_attach->layout;
                	subpass_descs[i].pDepthStencilAttachment = attach_refs + attach_ct;
                	attach_ct++;
        	}
	}

	assert(attach_ct < RPASS_MAX_ATTACHMENTS);

	// Create render pass
        VkRenderPassCreateInfo info = {0};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = attach_ct;
        info.pAttachments = attach_descs;
        info.subpassCount = subpass_ct;
        info.pSubpasses = subpass_descs;
        info.dependencyCount = dependency_ct;
        info.pDependencies = dependencies;

        VkResult res = vkCreateRenderPass(device, &info, NULL, rpass);
        assert(res == VK_SUCCESS);
}

#endif // LL_RPASS_H
