#ifndef LL_SHADER_H
#define LL_SHADER_H

#include <vulkan/vulkan.h>

#include <assert.h>
#include <stdio.h>

VkShaderModule load_shader(VkDevice device, const char* path) {
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

        VkShaderModule shader;
        VkResult res = vkCreateShaderModule(device, &info, NULL, &shader);
        assert(res == VK_SUCCESS);

        free(buf);

        return shader;
}

#endif // LL_SHADER_H

