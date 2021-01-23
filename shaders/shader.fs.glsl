#version 450

//layout (set = 1, binding = 0) uniform sampler2D tex_sampler;

layout (location = 0) in vec3 in_norm;
layout (location = 1) in vec2 in_tex_c;

layout (location = 0) out vec4 out_color;

void main() {
        out_color = vec4(in_norm, 1.0);
        // out_color = vec4(in_tex_c, 0.0, 1.0);
}
