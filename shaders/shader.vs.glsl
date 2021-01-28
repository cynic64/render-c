#version 450

layout (push_constant, std140) uniform PushConstants {
        mat4 model;
        vec3 ambient;
        vec3 diffuse;
        vec3 specular;
        bool use_textures;
} constants;

layout(set = 0, binding = 0) uniform Camera {
        mat4 view;
        mat4 proj;
} camera;

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_norm;
layout (location = 2) in vec2 in_tex_c;

layout (location = 0) out vec3 out_norm;
layout (location = 1) out vec2 out_tex_c;

void main() {
        out_norm = in_norm;
        out_tex_c = in_tex_c;
	gl_Position = camera.proj * camera.view * constants.model * vec4(in_pos, 1.0);
}
