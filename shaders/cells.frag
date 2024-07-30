#version 460

layout(location = 0) in float in_signed_distance;
layout(location = 0) out vec4 out_color;

void main() {
    // create complementary color gradient between negative and positive signed distances
    vec3 col_negative = vec3(0.0, 1.0, 0.0);
    vec3 col_positive = vec3(0.0, 0.0, 1.0);
    vec3 col_sd = mix(col_negative, col_positive, in_signed_distance * 0.5 + 0.5);

    // mark the "close-to-zero" with a bright red
    float sd_zero = clamp(in_signed_distance * 4, -1.0, 1.0);
    vec3 col_final = mix(vec3(1.0, 0.0, 0.0), col_sd, abs(sd_zero));

    float intensity = 1.0 - abs(in_signed_distance);
    out_color = vec4(col_final, intensity);
}