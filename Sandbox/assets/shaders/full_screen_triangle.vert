#version 450


layout(location = 0) out vec2 out_uv;

void main() {
    float x = -1.0 + 2.0 * float((gl_VertexIndex & 1) << 1);
    float y = -1.0 + 2.0 * float((gl_VertexIndex & 2));
    gl_Position = vec4(x, y, 0.0, 1.0);
    out_uv = vec2((x + 1.0) * 0.5, (y + 1.0) * 0.5);
}
