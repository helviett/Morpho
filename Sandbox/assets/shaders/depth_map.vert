#version 450

layout(location = 0) in vec2 in_position;

layout(location = 0) out vec2 out_uv;

void main() {
    gl_Position = vec4(vec3(in_position, 0.0), 1.0);
    out_uv = in_position * 0.5 + vec2(0.5);
}
