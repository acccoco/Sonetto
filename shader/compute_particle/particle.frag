#version 450


layout (binding = 0) uniform sampler2D samplerColorMap;
layout (binding = 1) uniform sampler2D samplerGradientRamp;

layout (location = 0) in float inGradientPos;
layout (location = 1) in vec3 inWorldPos;


layout (location = 0) out vec4 outFragColor;

void main()
{
    vec3 color = texture(samplerGradientRamp, vec2(inGradientPos, 0.0)).rgb;

    // gl_PointCoord 当前 fragment 相对于点图元的位置，范围是 [0, 1]
    // outFragColor.rgb = texture(samplerColorMap, gl_PointCoord).rgb * color;
    outFragColor.rgb = inWorldPos * 0.5 + 0.5;
}