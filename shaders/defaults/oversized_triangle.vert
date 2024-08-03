#version 460

// draw oversized triangle to achieve fullscreen fragment shader
void main() {
    int id = gl_VertexIndex;
    vec2 position;
    position.x = id == 1 ? 3.0 : -1.0;
    position.y = id == 2 ? 3.0 : -1.0;
    gl_Position = vec4(position, 1.0, 1.0);
}