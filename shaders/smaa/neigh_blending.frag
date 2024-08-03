#version 460

layout(location = 0) out vec4 out_color;
layout(binding = 0) uniform sampler2D tex_blend;

void main() {
    out_color = texelFetch(tex_blend, ivec2(gl_FragCoord.xy), 0);
}