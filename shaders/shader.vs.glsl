#version 450

layout (location = 0) in vec2 in_pos;
layout (location = 1) in vec3 in_color;

layout (binding = 0) uniform UBO {
        mat4 model;
        mat4 view;
        mat4 proj;
} mvp;

layout (location = 0) out vec3 out_color;

void main() {
	out_color = in_color;
	gl_Position = mvp.proj * mvp.view * mvp.model * vec4(in_pos, 0.0, 1.0);
}
