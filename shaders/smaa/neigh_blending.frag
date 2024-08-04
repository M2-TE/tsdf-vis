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

// something here doesnt quite work..
vec4 SMAANeighborhoodBlendingPS2(
    vec2 texcoord, 
    vec4 offset, 
    SMAATexture2D(colorTex), 
    SMAATexture2D(blendTex))
{
    // Fetch the blending weights for current pixel:
    vec4 a;
    a.x = texture(blendTex, offset.xy).a; // Right
    a.y = texture(blendTex, offset.zw).g; // Top
    a.wz = texture(blendTex, texcoord).xz; // Bottom / Left

    // Is there any blending weight with a value greater than 0.0?
    if (dot(a, vec4(1.0, 1.0, 1.0, 1.0)) < 1e-5) {
        vec4 color = SMAASampleLevelZero(colorTex, texcoord);
        return color;
    } else {
        bool h = max(a.x, a.z) > max(a.y, a.w); // max(horizontal) > max(vertical)

        // Calculate the blending offsets:
        vec4 blendingOffset = vec4(0.0, a.y, 0.0, a.w);
        vec2 blendingWeight = a.yw;
        SMAAMovc(bool4(h, h, h, h), blendingOffset, vec4(a.x, 0.0, a.z, 0.0));
        SMAAMovc(bool2(h, h), blendingWeight, a.xz);
        blendingWeight /= dot(blendingWeight, vec2(1.0, 1.0));

        // Calculate the texture coordinates:
        vec4 blendingCoord = mad(blendingOffset, vec4(SMAA_RT_METRICS.xy, -SMAA_RT_METRICS.xy), texcoord.xyxy);

        // We exploit bilinear filtering to mix current pixel with the chosen
        // neighbor:
        vec4 color = blendingWeight.x * SMAASampleLevelZero(colorTex, blendingCoord.xy);
        color += blendingWeight.y * SMAASampleLevelZero(colorTex, blendingCoord.zw);
        
        return color;
    }
}

void main() {
    out_color = SMAANeighborhoodBlendingPS2(
        in_texcoord, in_offset,
        tex_color, tex_blend);
}