#version 460

layout(location = 0) in float in_signed_distance;
layout(location = 0) out vec4 out_color;

void main() {
    out_color = vec4(1.0, 0.0, 0.0, 1.0);
}