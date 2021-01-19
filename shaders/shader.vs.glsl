#version 450

layout (location = 0) in vec2 in_pos;
layout (location = 1) in vec3 in_color;
layout (location = 2) in vec2 in_tex_c;

layout (binding = 0) uniform UBO {
        mat4 model;
        mat4 view;
        mat4 proj;
} mvp;

layout (location = 0) out vec3 out_color;
layout (location = 1) out vec2 out_tex_c;

void main() {
	out_color = in_color;
	out_tex_c = in_tex_c;
	gl_Position = mvp.proj * mvp.view * mvp.model * vec4(in_pos, 0.0, 1.0);
}
