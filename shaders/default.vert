#version 460

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 0) out vec3 out_normal;

// Camera view and projection matrix
layout(set = 0, binding = 0) uniform Camera {
    mat4x4 matrix;
} camera;

void main() {
    gl_Position = vec4(in_position, 1.0);
    gl_Position = camera.matrix * gl_Position;
    out_normal = in_normal;
}