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

float A = 0.15;
float B = 0.50;
float C = 0.10;
float D = 0.20;
float E = 0.02;
float F = 0.30;
float W = 11.2;

// taken from: http://filmicworlds.com/blog/filmic-tonemapping-operators/
vec3 Uncharted2Tonemap(vec3 x) {
        return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

void main() {
        vec3 light_dir = normalize(vec3(1.0, 1.0, 1.0));

        vec3 diffuse_color = texture(tex_sampler, vec2(in_tex_c.x, in_tex_c.y * -1.0)).rgb * constants.use_textures
                + constants.diffuse * (1.0 - constants.use_textures);
        float diffuse_factor = max(dot(in_norm, light_dir), 0.0);

        vec3 ambient_color = diffuse_color;
        float ambient_factor = 0.1;

        vec3 result = diffuse_color * diffuse_factor + ambient_color * ambient_factor;
        result *= 2.0;
        float exposure_bias = 1.0;
        vec3 corrected = pow(Uncharted2Tonemap(exposure_bias * result), vec3(1.0/2.2));
        out_color = vec4(corrected, 1.0);
}
