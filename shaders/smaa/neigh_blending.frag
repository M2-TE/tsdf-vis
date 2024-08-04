#version 460
#extension GL_ARB_shading_language_include: require
#define SMAA_INCLUDE_VS 0
#define SMAA_INCLUDE_PS 1
#include "smaa/settings.h"

layout(location = 0) in vec2 in_texcoord;
layout(location = 1) in vec4 in_offset;
layout(location = 0) out vec4 out_color;
layout(binding = 0) uniform sampler2D tex_color;
layout(binding = 1) uniform sampler2D tex_blend;

void main() {
    out_color = SMAANeighborhoodBlendingPS(in_texcoord, in_offset, tex_color, tex_blend);
}