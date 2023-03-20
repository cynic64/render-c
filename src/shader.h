#ifndef LL_SHADER_H
#define LL_SHADER_H

#include <vulkan/vulkan.h>

#include <assert.h>
#include <stdio.h>
#include <vulkan/vulkan_core.h>
#include <stdlib.h>
#include <string.h>

// If info is NULL, stage won't be used.
void load_shader(VkDevice device, const char* path, VkShaderModule* module,
		 VkShaderStageFlagBits stage, VkPipelineShaderStageCreateInfo* pipeline_info) {
        FILE *fp = fopen(path, "r");
        assert(fp != NULL);

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

        VkResult res = vkCreateShaderModule(device, &info, NULL, module);
        assert(res == VK_SUCCESS);

        free(buf);

	if (pipeline_info != NULL) {
		bzero(pipeline_info, sizeof(*pipeline_info));
		pipeline_info->sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		pipeline_info->stage = stage;
		pipeline_info->module = *module;
		pipeline_info->pName = "main";
	}
}

#endif // LL_SHADER_H

