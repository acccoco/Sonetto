#version 450


layout (location = 0) in float in_height;
layout (location = 0) out vec4 out_color;


void main()
{
    // 根据高度来决定颜色
    // 高度范围是 [-16.0, 48.0]
    out_color = vec4(vec3((in_height + 16.0) / 64.0), 1.0);
}