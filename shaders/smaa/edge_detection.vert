#version 460
#extension GL_ARB_shading_language_include : require

#define SMAA_RT_METRICS vec4(1.0 / 1280.0, 1.0 / 720.0, 1280.0, 720.0)
#define SMAA_INCLUDE_VS 1
#define SMAA_INCLUDE_PS 0
#define SMAA_GLSL_4
#define SMAA_PRESET_HIGH
#include "SMAA.hlsl"

layout(location = 0) out vec2 out_texcoord;
layout(location = 1) out vec4 out_offsets[3];

void main() {
    // oversized triangle
    int id = gl_VertexIndex;
    vec2 position;
    position.x = id == 1 ? 3.0 : -1.0;
    position.y = id == 2 ? 3.0 : -1.0;
    gl_Position = vec4(position, 0.0, 1.0);
    out_texcoord.x = id == 1 ? 2.0 : 0.0;
    out_texcoord.y = id == 2 ? 2.0 : 0.0;
    SMAAEdgeDetectionVS(out_texcoord, out_offsets);
}