#version 460 core

void main() {
    float x = float(gl_VertexID & 1);
    float y = float(gl_VertexID >> 1);

    vec2 uv = 2*vec2(x, y)-1;
    gl_Position = vec4(uv, 0.0, 1.0);
}
