#ifndef SHADER_COMMON
#define SHADER_COMMON

#extension GL_GOOGLE_include_directive : enable
#include "common_type.glsl"


/**
 * 由深度值（范围 [0, 1]）得到 view space 中的 z 坐标（负数）
 * near 和 far 都是正数
 * 验证：depth = 0，得到 -near
 *       depth = 1，得到 -far
 */
float depth_to_view_z(float depth, float near, float far)
{
    return near * far / ((far - near) * depth - far);
}


/**
 * 从 screen space 变换到 view space
 * 参考：https://www.khronos.org/opengl/wiki/Compute_eye_space_from_window_space
 * @param frag_coord 需要满足 gl_FragCoord 的要求
 * @param screen_size 屏幕的尺寸
 * @param in_proj 逆投影变换矩阵
 */
vec4 screen_to_view(vec4 frag_coord, vec2 screen_size, mat4 in_proj)
{
    // 首先变换到 NDC: [-1, 1] * [-1, 1] * [0, 1]
    vec4 NDC_coord = vec4(frag_coord.xy / screen_size * 2.0 - 1.0, frag_coord.z, 1);

    // 变换到 clip，需要真实的深度值
    vec4 clip_coord = NDC_coord / frag_coord.w;
    vec4 view_coord = in_proj * clip_coord;

    return view_coord / view_coord.w;
}


/**
 * 对 ACES 曲线的快速拟合
 * https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
 */
vec3 ACES_HDR2SDR(vec3 hdr)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((hdr * (a * hdr + b)) / (hdr * (c * hdr + d) + e), 0.0, 1.0);
}


/**
 * 计算颜色对应的亮度
 */
float get_brightness(vec3 color)
{
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}


#endif    // SHADER_COMMON