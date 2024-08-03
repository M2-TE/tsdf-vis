#version 460
#extension GL_ARB_shading_language_include : require

#define SMAA_RT_METRICS vec4(1.0 / 1280.0, 1.0 / 720.0, 1280.0, 720.0)
#define SMAA_INCLUDE_VS 0
#define SMAA_INCLUDE_PS 1
#define SMAA_GLSL_4
#define SMAA_PRESET_HIGH
#include "SMAA.hlsl"

layout(location = 0) in vec2 in_texcoord;
layout(location = 1) in vec4 in_offset;
layout(location = 0) out vec4 out_color;
layout(binding = 0) uniform sampler2D tex_blend;
layout(binding = 1) uniform sampler2D tex_color;

void main() {
    out_color = SMAANeighborhoodBlendingPS(in_texcoord, in_offset, tex_blend, tex_color);
    // out_color = texture(tex_blend, in_texcoord);
}