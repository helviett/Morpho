#version 450

layout(set = 1, binding = 0) uniform LightViewPorjectionBlock {
    mat4 view;
    mat4 proj;
} lvp;
layout(set = 1, binding = 2) uniform sampler2D shadow_map;

layout(location = 0) in vec2 in_uv;

layout(location = 0) out vec4 out_color;

float linearize_depth(float z, float n, float f)
{
    return (2.0 * n) / (f + n - z * (f - n));
}

void main() {
    float x = lvp.proj[2][2];
    float y = lvp.proj[3][2];
    float near = -y / x;
    float far = -y / (x - 1);
    float depth = texture(shadow_map, in_uv).r;
    out_color = vec4(vec3(linearize_depth(depth, near, far)), 1.0);
}
