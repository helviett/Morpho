#version 450

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

void main() {
    gl_Position = lvp.proj * lvp.view * model.t * vec4(in_position, 1.0);
}
