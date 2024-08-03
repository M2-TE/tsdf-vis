#version 460

layout(location = 0) out vec4 out_blend;
layout(binding = 0) uniform sampler2D tex_edges;

void main() {
    out_blend = texelFetch(tex_edges, ivec2(gl_FragCoord.xy), 0);
}