#version 450
#extension GL_GOOGLE_include_directive : enable
#include "globals.h"
#include "pbr.h"

struct SpotLight {
    vec3 position;
    vec3 direction;
    vec3 color;
    float radius;
    float intensity;
    float umbra;
    float penumbra;
};

layout(set = 1, binding = 1, std140) uniform SpotLightBlock {
    SpotLight data;
} spot_light;
layout(set = 1, binding = 2) uniform sampler2DShadow shadow_map;

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
layout(location = 4) in vec4 in_light_space_position;

layout(location = 0) out vec4 out_color;

vec3 fetch_normal_vector() {
    vec3 m = normalize(texture(normal_texture, in_uv).xyz * 2.0 - 1.0);
    vec3 n = normalize(in_normal);
    vec3 t = normalize(in_tangent.xyz - dot(in_tangent.xyz, n) * n);
    vec3 b = in_tangent.w * cross(n, t);
    return m.x * t + m.y * b + m.z * n;
}

float calculate_distance_attenuation(float radius, float max_radius) {
    return pow(max(1 - pow(radius / max_radius, 4), 0), 2);
}

vec3 calculate_spot_light(SpotLight light, vec3 albedo, vec3 normal, vec3 view_dir, float metalness, float roughness) {
    vec3 light_dir = light.direction;
    float r = length(in_position - light.position);
    vec3 color = calculate_light(-light_dir, normal, view_dir, albedo, metalness, roughness);
    float distance_attenuation = calculate_distance_attenuation(r, light.radius);
    vec3 direction = normalize(in_position - light.position);
    float theta = max(0, dot(direction, light.direction));
    float epsilon = light.penumbra - light.umbra;
    float angle_attenuation = clamp((theta - light.umbra) / epsilon, 0.0, 1.0);
    return light.intensity * distance_attenuation * angle_attenuation * color * light.color.rgb;
}

float calculate_shadow(vec4 position) {
    vec3 proj_coords = position.xyz / position.w;
    proj_coords = proj_coords * vec3(0.5, 0.5, 1.0) + vec3(0.5, 0.5, 0.0);
    float offset = 1.0 / textureSize(shadow_map, 0).r;
    float current_surface_depth = proj_coords.z;
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

void main() {
    vec3 base_color = texture(base_color_texture, in_uv).rgb * material.base_color_factor.rgb;
    vec2 metalness_roughness = texture(metalness_roughness_texture, in_uv).bg * material.metalness_roughness_factor;
    vec3 normal = fetch_normal_vector();
    vec3 view_dir = normalize(globals.position - in_position);
    vec3 ambient = 0.5 * base_color;
    vec3 light = calculate_spot_light(spot_light.data, base_color, normal, view_dir, metalness_roughness.x, metalness_roughness.y);
    float shadow = calculate_shadow(in_light_space_position);
    out_color = vec4(light * shadow, 1.0);
}
