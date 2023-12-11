#version 450
#extension GL_GOOGLE_include_directive : enable
#include "globals.h"

layout(set = 3, binding = 0) uniform ModelBlock {
    mat4 t;
} model;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec4 in_tangent;

void main() {
    gl_Position = globals.proj * globals.view * model.t * vec4(in_position, 1.0);
}
