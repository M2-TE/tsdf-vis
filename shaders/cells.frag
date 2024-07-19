#version 460

layout(location = 0) in float in_signed_distance;
layout(location = 0) out vec4 out_color;

void main() {
    vec3 col_negative = vec3(0.0, 1.0, 0.0);
    vec3 col_positive = vec3(0.0, 0.0, 1.0);
    float interpolater = in_signed_distance * 0.5 + 0.5;

    float intensity = 1.0 - abs(in_signed_distance);

    out_color = vec4(mix(col_negative, col_positive, interpolater), intensity);
    // out_color = vec4(0.0, 0.0, 1.0, 1.0);
}