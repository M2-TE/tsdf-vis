#version 460

void main() {
    int id = gl_VertexIndex;
    vec2 position = vec2((id << 1) & 2, id & 2);
    gl_Position = vec4(position, 1.0, 1.0);
}