#version 450

layout (set = 0, binding = 1) uniform sampler2D tex_sampler;

layout (location = 0) in vec2 in_tex_c;

layout (location = 0) out vec4 out_color;

void main() {
        out_color = texture(tex_sampler, in_tex_c);
}
