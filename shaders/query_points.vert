#version 460

layout(location = 0) in vec3 in_position;
layout(location = 1) in float in_signed_distance;
layout(location = 0) out float out_signed_distance;

// Camera view and projection matrix
layout(set = 0, binding = 0) uniform Camera {
    mat4x4 matrix;
} camera;

void main() {
    gl_Position = vec4(in_position, 1.0);
    gl_Position = camera.matrix * gl_Position;
    gl_PointSize = 3.0;
    out_signed_distance = in_signed_distance;
}