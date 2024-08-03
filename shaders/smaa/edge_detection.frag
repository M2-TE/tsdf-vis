#version 460
#extension GL_ARB_shading_language_include : require
#define SMAA_RT_METRICS vec4(1.0 / 1280.0, 1.0 / 720.0, 1280.0, 720.0)
#define SMAA_INCLUDE_VS 0
#define SMAA_INCLUDE_PS 1
#define SMAA_GLSL_4
#define SMAA_PRESET_HIGH
#include "SMAA.hlsl"

layout(location = 0) out vec4 out_color;
layout(binding = 0) uniform sampler2D tex_edges;

void main() {
    out_color = texelFetch(tex_edges, ivec2(gl_FragCoord.xy), 0);
}