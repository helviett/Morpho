#version 450

layout(set = 0, binding = 0) uniform ViewProjectionBlock {
    mat4 view;
    mat4 proj;
} vp;

layout(set = 1, binding = 0) uniform LightViewPorjectionBlock {
    mat4 view;
    mat4 proj;
} lvp;

layout(set = 3, binding = 0) uniform ModelBlock {
    mat4 t;
} model;


layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec4 in_tangent;

layout(location = 0) out vec3 out_position;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec2 out_uv;
layout(location = 3) out vec4 out_tangent;
layout(location = 4) out vec3 out_light_space_position;

void main() {
    vec4 position = model.t * vec4(in_position, 1.0);
    gl_Position = vp.proj * vp.view * position;
    out_position = position.xyz;
    out_uv = in_uv;
    out_normal = (model.t * vec4(in_normal, 0.0)).xyz;
    out_tangent = model.t * vec4(in_tangent.xyz, 0.0);
    out_tangent.w = in_tangent.w;
    out_light_space_position = (lvp.view * position).xyz;
}
