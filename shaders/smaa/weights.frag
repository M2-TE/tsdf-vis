#version 460
#extension GL_ARB_shading_language_include: require
#extension GL_EXT_control_flow_attributes: require
#define SMAA_INCLUDE_VS 0
#define SMAA_INCLUDE_PS 1
#include "smaa/settings.glsl"

layout(location = 0) in vec2 in_texcoord;
layout(location = 1) in vec2 in_pixcoord;
layout(location = 2) in vec4 in_offsets[3];
layout(location = 0) out vec4 out_weights;
layout(binding = 0) uniform sampler2D tex_edges;
layout(binding = 1) uniform sampler2D tex_area;
layout(binding = 2) uniform sampler2D tex_search;

void main() {
    out_weights = SMAABlendingWeightCalculationPS(
        in_texcoord, in_pixcoord, in_offsets, 
        tex_edges, tex_area, tex_search, 
        vec4(0, 0, 0, 0));
}