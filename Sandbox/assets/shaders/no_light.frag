#version 450

layout(set = 1, binding = 0) uniform sampler2D base_color_texture;
layout(set = 1, binding = 1) uniform Materal {
    vec4 base_color_factor;
} material;
layout(set = 1, binding = 4) uniform sampler2D normal_texture;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec4 in_tangent;

layout(location = 0) out vec4 out_color;

void main() {
    vec3 base_color = texture(base_color_texture, in_uv).rgb * material.base_color_factor.rgb;
    out_color = vec4(base_color, 1.0);
}
