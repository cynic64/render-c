#version 450

layout (push_constant, std140) uniform PushConstants {
        mat4 model;
        vec4 ambient;
        vec4 diffuse;
        vec4 specular;
        float use_textures;
} constants;

layout (set = 1, binding = 0) uniform sampler2D tex_sampler;

layout (location = 0) in vec3 in_norm;
layout (location = 1) in vec2 in_tex_c;

layout (location = 0) out vec4 out_color;

void main() {
        vec3 light_dir = normalize(vec3(100.0, 100.0, 100.0));

        vec4 diffuse_color = texture(tex_sampler, vec2(in_tex_c.x, in_tex_c.y * -1.0)) * constants.use_textures
                + constants.diffuse * (1.0 - constants.use_textures);
        float diffuse_factor = max(dot(in_norm, light_dir), 0.0);

        vec4 ambient_color = diffuse_color;
        float ambient_factor = 0.1;

        out_color = diffuse_color * diffuse_factor + ambient_color * ambient_factor;
}
