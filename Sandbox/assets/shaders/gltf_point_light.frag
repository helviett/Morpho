#version 450

struct PointLight {
    vec3 position;
    vec3 color;
    float radius;
    float intensity;
};

layout(set = 1, binding = 0) uniform LightViewPorjectionBlock {
    mat4 view;
    mat4 proj;
} lvp;
layout(set = 1, binding = 1, std140) uniform PointLightBlock {
    PointLight data;
} point_light;
layout(set = 1, binding = 2) uniform samplerCubeShadow shadow_map;

layout(set = 2, binding = 0) uniform Materal {
    vec4 base_color_factor;
} material;
layout(set = 2, binding = 1) uniform sampler2D base_color_texture;
layout(set = 2, binding = 2) uniform sampler2D normal_texture;



layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec4 in_tangent;
layout(location = 4) in vec3 in_light_space_position;

layout(location = 0) out vec4 out_color;

vec3 fetch_normal_vector() {
    vec3 m = texture(normal_texture, in_uv).xyz;
    vec3 n = normalize(in_normal);
    vec3 t = normalize(in_tangent.xyz - dot(in_tangent.xyz, n) * n);
    vec3 b = in_tangent.w * cross(n, t);
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

vec3 calculate_point_light(PointLight light, vec3 normal, vec3 view_dir) {
    vec3 light_dir = light.position - in_position;
    float r = length(light_dir);
    vec3 color = calculate_light(normalize(light_dir), normal, view_dir, light.color.rgb);
    float attenuation = calculate_distance_attenuation(r, light.radius);
    return light.intensity * attenuation * color;
}

float calculate_shadow(vec3 position) {
    vec3 abs_position = abs(position);
    float max_magnitue = max(abs_position.x, max(abs_position.y, abs_position.z));
    float m22 = lvp.proj[2][2];
    float m23 = lvp.proj[3][2];
    float depth = m22 + m23 / max_magnitue;
    float shadow = texture(shadow_map, vec4(position, depth));
    return shadow;
}

void main() {
    vec3 base_color = texture(base_color_texture, in_uv).rgb * material.base_color_factor.rgb;
    vec3 normal = fetch_normal_vector();
    vec3 color = calculate_point_light(point_light.data, normal, -in_light_space_position.xyz);
    float shadow = calculate_shadow(in_light_space_position.xyz);
    out_color = vec4(color * base_color * shadow, 1.0);
}
