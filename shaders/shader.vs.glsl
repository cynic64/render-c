#version 450

layout (push_constant) uniform PushConstants {
        mat4 model;
        mat4 proj_view;
} mvp;

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_norm;
layout (location = 2) in vec2 in_tex_c;

layout (location = 0) out vec3 out_norm;
layout (location = 1) out vec2 out_tex_c;

void main() {
        out_norm = in_norm;
        out_tex_c = in_tex_c;
	gl_Position = mvp.proj_view * mvp.model * vec4(in_pos, 1.0);
}
