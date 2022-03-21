#version 450

layout(set = 0, binding = 1) uniform sampler2D base_color_texture;
layout(set = 0, binding = 2) uniform Materal {
    vec4 base_color_factor;
} material;

layout(location = 0) in vec3 normal;
layout(location = 1) in vec2 uv;

layout(location = 0) out vec4 out_color;

void main() {
    out_color = texture(base_color_texture, uv) * material.base_color_factor;
}
