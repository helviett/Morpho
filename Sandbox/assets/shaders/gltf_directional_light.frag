#version 450
#extension GL_GOOGLE_include_directive : enable
#include "globals.h"
#include "pbr.h"

struct DirectionalLight {
    vec3 direction;
    vec3 color;
};

#define CASCADE_COUNT 3

layout(set = 1, binding = 0) uniform CsmBlock {
    mat4 first_cascade_view_proj;
    vec2 ranges[CASCADE_COUNT];
    vec3 offsets[CASCADE_COUNT];
    vec3 scales[CASCADE_COUNT];
} csm;

layout(set = 1, binding = 1, std140) uniform DirectionalLightBlock {
    DirectionalLight data;
} dir_light;
layout(set = 1, binding = 2) uniform sampler2DArrayShadow shadow_map;

layout(set = 2, binding = 0) uniform Materal {
    vec4 base_color_factor;
    vec2 metalness_roughness_factor;
} material;
layout(set = 2, binding = 1) uniform sampler2D base_color_texture;
layout(set = 2, binding = 2) uniform sampler2D normal_texture;
layout(set = 2, binding = 3) uniform sampler2D metalness_roughness_texture;


layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec4 in_tangent;
layout(location = 4) in float in_view_depth;
layout(location = 5) in vec3 in_shadow_map_uv;

layout(location = 0) out vec4 out_color;

vec3 fetch_normal_vector() {
    vec3 m = normalize(texture(normal_texture, in_uv).xyz * 2.0 - 1.0);
    vec3 n = normalize(in_normal);
    vec3 t = normalize(in_tangent.xyz - dot(in_tangent.xyz, n) * n);
    vec3 b = sign(in_tangent.w) * cross(n, t);
    return m.x * t + m.y * b + m.z * n;
}

float calculate_distance_attenuation(float radius, float max_radius) {
    return pow(max(1 - pow(radius / max_radius, 4), 0), 2);
}

vec3 calculate_light(vec3 light_dir, vec3 normal, vec3 view_dir, vec3 color) {
    float diffuse_power = max(dot(light_dir, normal), 0.0);
    vec3 halfway_dir = normalize(light_dir + view_dir);
    float specular_power = pow(max(dot(normal, halfway_dir), 0), 32.0);
    specular_power = 0;
    return diffuse_power * color + specular_power * color;
}

vec3 calculate_directional_light(DirectionalLight light, vec3 albedo, vec3 normal, vec3 view_dir, float metalness, float roughness) {
    vec3 light_dir = light.direction;
    vec3 color = calculate_light(-light_dir, normal, view_dir, albedo, metalness, roughness);
    return light.color.rgb * color * 3.0f;
}

float calculate_shadow(vec3 shadow_map_uv, uint cascade_index) {
    shadow_map_uv *= csm.scales[cascade_index];
    shadow_map_uv += csm.offsets[cascade_index];
    vec4 proj_coords = vec4(shadow_map_uv, 0.0);
    proj_coords.xy = proj_coords.xy * vec2(0.5, 0.5) + vec2(0.5, 0.5);
    proj_coords.zw = vec2(cascade_index, proj_coords.z);
    float offset = 1.0 / textureSize(shadow_map, 0).r;
    float current_surface_depth = proj_coords.w;
    float light = texture(shadow_map, proj_coords);
    proj_coords.xy -= offset;
    light += texture(shadow_map, proj_coords);
    proj_coords.x += offset * 2.0;
    light += texture(shadow_map, proj_coords);
    proj_coords.y += offset * 2.0;
    light += texture(shadow_map, proj_coords);
    proj_coords.x -= offset * 2.0;
    light += texture(shadow_map, proj_coords);
    return light * 0.2;
}

const vec3 cascade_colors[CASCADE_COUNT] =
{
    vec3(1.0f, 0.0, 0.0f),
    vec3(0.0f, 1.0f, 0.0f),
    vec3(0.0f, 0.0f, 1.0f),
};

void main() {
    vec3 base_color = texture(base_color_texture, in_uv).rgb * material.base_color_factor.rgb;
    vec2 metalness_roughness = texture(metalness_roughness_texture, in_uv).bg * material.metalness_roughness_factor;
    vec3 normal = fetch_normal_vector();
    vec3 view_dir = normalize(globals.position - in_position);
    vec3 ambient = 0.5 * base_color;
    uint cascade_index = CASCADE_COUNT - 1;
    for (uint i = 0; i < CASCADE_COUNT; i++) {
        if (in_view_depth <= csm.ranges[i].y) {
            cascade_index = i;
            break;
        }
    }
    vec3 light = calculate_directional_light(dir_light.data, base_color, normal, view_dir, metalness_roughness.x, metalness_roughness.y);
    float shadow = calculate_shadow(in_shadow_map_uv, cascade_index);
    if (cascade_index != CASCADE_COUNT - 1 && csm.ranges[cascade_index + 1].x < in_view_depth) {
        float next_sm_shadow = calculate_shadow(in_shadow_map_uv, cascade_index + 1);
        float ratio = (in_view_depth - csm.ranges[cascade_index + 1].x) / (csm.ranges[cascade_index].y - csm.ranges[cascade_index + 1].x );
        shadow = mix(shadow, next_sm_shadow, ratio);
    }
    out_color = vec4(light * shadow, 1.0);
    //out_color.xyz *= cascade_colors[cascade_index];
    out_color.xyz += ambient;
    //out_color.xyz = normal * 0.5 + 0.5;
    // tonemapping:
    out_color.rgb = out_color.rgb / (out_color.rgb + vec3(1.0));
}
