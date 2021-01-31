#version 450

layout (push_constant, std140) uniform PushConstants {
        mat4 model;
        vec3 ambient;
        float pad1;
        vec3 diffuse;
        float pad2;
        vec3 specular;
        float pad3;
        float use_textures;
} constants;

layout (set = 1, binding = 0) uniform sampler2D tex_sampler;

layout (location = 0) in vec3 in_norm;
layout (location = 1) in vec2 in_tex_c;

layout (location = 0) out vec4 out_color;

void main() {
        if (constants.use_textures != 0.0) {
                out_color = texture(tex_sampler, vec2(in_tex_c.x, in_tex_c.y * -1.0));
        } else {
                out_color = vec4(1.0, 0.0, 1.0, 1.0);
        }
}
