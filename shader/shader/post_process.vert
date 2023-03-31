// 用于后处理的着色器
#version 450


layout(location = 0) in vec2 in_pos;
layout(location = 0) out vec2 out_uv;


void main()
{
    gl_Position = vec4(in_pos, 0, 1);

    // vulkan 的 framebuffer 原点位于左上角
    // 纹理坐标系的原点恰好也在左上角
    out_uv = in_pos;
}