#version 460
#extension GL_ARB_shading_language_include : require

#define SMAA_RT_METRICS vec4(1.0 / 1280.0, 1.0 / 720.0, 1280.0, 720.0)
#define SMAA_INCLUDE_VS 0
#define SMAA_INCLUDE_PS 1
#define SMAA_GLSL_4
#define SMAA_PRESET_HIGH
#include "SMAA.hlsl"

layout(location = 0) in vec2 in_texcoord;
layout(location = 1) in vec4 in_offsets[3];
layout(location = 0) out vec4 out_edges;
layout(binding = 0) uniform sampler2D tex_color;

void main() {
    // vec2 edge = SMAALumaEdgeDetectionPS(in_texcoord, in_offsets, tex_color);
    vec2 edges = SMAAColorEdgeDetectionPS(in_texcoord, in_offsets, tex_color);
    out_edges = vec4(edges, 0.0, 0.0); // TODO: reduce output texture to 2 channels

    // out_edges = texelFetch(tex_color, ivec2(gl_FragCoord.xy), 0);
    // out_edges = texture(tex_color, in_texcoord);
}