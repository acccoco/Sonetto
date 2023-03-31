#version 450
#extension GL_GOOGLE_include_directive : enable
#include "../shader/common.glsl"
#include "type.glsl"
#include "../shader/material.glsl"

layout(location = 0) in VertFrag vs;
layout(location = 0) out vec4 out_color;


// 使用预先定义的几个纹理
TEXTURE_BINDING(1, 0)
layout(set = 2, binding = 0, std430) buffer _2_0_
{
    uint b_light_index_list[];    // 全局的 light list
};

// 每个 tile 对应的 light list 的 offset 和 count
layout(set = 2, binding = 1, rg32ui) uniform uimage2D u_light_grid;
layout(set = 2, binding = 2, std430) buffer _2_2_
{
    Light b_lights[];
};


void main()
{
    // 获取物体的属性
    Material mat = u_mat;
    parse_material(mat, vs.uv, diffuse_texture, ambient_texture, specular_texture);

    // 获得常用向量
    vec3 eye_pos = vec3(0);    // 因为是 view space

    vec3 V = normalize(eye_pos - vs.pos_view);    // 观察方向
    vec3 P = vs.pos_view;                         // fragment 在 view space 中的位置
    vec3 N = normalize(vs.normal_view);           // view space 中的法线方向

    // 根据 framebuffer 的坐标得到 tile 的 index
    ivec2 tile_id = ivec2(gl_FragCoord.xy) / int(TILE_SIZE);

    uvec2 temp               = imageLoad(u_light_grid, tile_id).xy;
    uint  light_index_offset = temp.x;
    uint  light_count        = temp.y;

    LightingResult lit = LightingResult(vec3(0), vec3(0));    // 所有光源的照明效果

    // 遍历每一个光源
    for (uint i = 0; i < light_count; i += 1)
    {
        uint  light_index = b_light_index_list[light_index_offset + i];
        Light light       = b_lights[light_index];

        LightingResult result = LightingResult(vec3(0), vec3(0));    // 当前光源的照明效果

        switch (light.type)
        {
            case DIRECTIONAL_LIGHT: result = do_directional_light(light, mat, V, P, N); break;

            case POINT_LIGHT: result = do_point_light(light, mat, V, P, N); break;
        }
        lit.diffuse += result.diffuse;
        // lit.specular += result.specular;
    }

    // vec3 ambient  = mat.ambient_color.rgb * mat.global_ambient.rgb;
    vec3 diffuse = mat.diffuse_color.rgb * lit.diffuse;
    // vec3 specular = mat.specular_color.rgb * lit.specular;

    vec3 color = diffuse / 2.f;
    out_color  = vec4(ACES_HDR2SDR(color), 1.0);
}