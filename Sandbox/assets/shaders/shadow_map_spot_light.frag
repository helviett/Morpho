#version 450

layout(set = 0, binding = 0) uniform sampler2D depth_map;
layout(set = 0, binding = 1) uniform ProjectionParameters {
    float near_plane;
    float far_plane;
} projection_parameters;

layout(location = 0) in vec2 in_uv;

layout(location = 0) out vec4 out_color;

float linearize_depth(float depth)
{
    float n = projection_parameters.near_plane;
    float f = projection_parameters.far_plane;
    float z = depth;
    return (2.0 * n) / (f + n - z * (f - n));
}

void main() {
    float depth = texture(depth_map, in_uv).r;
    out_color = vec4(vec3(linearize_depth(depth)), 1.0);
}
