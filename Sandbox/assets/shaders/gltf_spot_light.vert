#version 450
#extension GL_GOOGLE_include_directive : enable
#include "globals.h"
// set 0 -- globals
// set 1 -- per light
// set 2 -- per material
// set 3 -- per draw

layout(set = 1, binding = 0) uniform LightViewPorjectionBlock {
    mat4 view;
    mat4 proj;
} lvp;

layout(set = 3, binding = 0) uniform ModelBlock {
    mat4 transform;
    mat4 inv_transpose_transform;
} model;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec4 in_tangent;

layout(location = 0) out vec3 out_position;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec2 out_uv;
layout(location = 3) out vec4 out_tangent;
layout(location = 4) out vec4 out_light_space_position;

void main() {
    vec4 position = model.transform * vec4(in_position, 1.0);
    gl_Position = globals.proj * globals.view * position;
    out_position = position.xyz;
    out_uv = in_uv;
    out_normal = (model.inv_transpose_transform * vec4(in_normal, 0.0)).xyz;
    out_tangent = model.transform * vec4(in_tangent.xyz, 0.0);
    out_tangent.w = in_tangent.w;
    out_light_space_position = lvp.proj * lvp.view * position;
}
