/**
 * 后处理：简单的将两个纹理合并，并做 hdr - sdr 变换
 */
#version 450
#extension GL_GOOGLE_include_directive : enable
#include "../shader/common.glsl"
#include "../shader/light.glsl"

layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 out_frag_color;

layout(set = 0, binding = 0) uniform sampler2D frag_color_tex;
layout(set = 0, binding = 1) uniform sampler2D bloom_color_tex;


void main()
{
    vec3 frag_color  = texture(frag_color_tex, in_uv).xyz;
    vec3 bloom_color = texture(bloom_color_tex, in_uv).xyz;

    out_frag_color = vec4(ACES_HDR2SDR(frag_color + bloom_color), 1.0);
}