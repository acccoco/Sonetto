#version 450
#extension GL_GOOGLE_include_directive: enable
#include "./common.glsl"
#define USE_TEX_SLOT
#include "../include/light.glsl"


layout (location = 0) in vary
{
    vec2 uv;
    vec3 pos_view;  // view space 中物体的位置
    vec3 normal_view;
};

layout (location = 0) out vec4 out_color;


layout (std430, set = 0, binding = TEX_SLOT_NUM + 0) buffer _0
{
    uint b_light_index_list[];    // 全局的 light list
};
layout (std430, set = 0, binding = TEX_SLOT_NUM + 1) buffer _1
{
    uvec2 b_light_grid[];    // 每个 tile 对应的 light list 的 offset 和 count
};
layout (std140, set = 0, binding = TEX_SLOT_NUM + 2) uniform _2
{
    uvec2 u_tile_num;       // x 方向和 y 方向 tile 的数量
};
layout (std430, set = 0, binding = TEX_SLOT_NUM + 3) buffer _3
{
    Light b_lights[];
};


void main()
{
    // 获取物体的属性
    Material mat = u_mat;
    parse_material(mat, uv);

    // 获得常用向量
    vec3 eye_pos = vec3(0);     // 因为是 view space
    vec3 V = normalize(eye_pos - pos_view);     // 观察方向
    vec3 P = pos_view;                          // fragment 在 view space 中的位置
    vec3 N = normalize(normal_view);            // view space 中的法线方向


    // 根据 framebuffer 的坐标得到 tile 的 index
    uvec2 tile_id = uvec2(floor(gl_FragCoord.xy / TILE_SIZE));
    uint tile_index = tile_id.x + tile_id.y * u_tile_num.x;

    uint light_index_offset = b_light_grid[tile_index].x;
    uint light_count = b_light_grid[tile_index].y;

    LightingResult lit;     // 所有光源的照明效果

    // 遍历每一个光源
    for (uint i = 0; i < light_count; ++i)
    {
        uint light_index = b_light_index_list[light_index_offset + i];
        Light light = b_lights[light_index];

        LightingResult result = LightingResult(vec3(0), vec3(0));  // 当前光源的照明效果

        switch (light.type)
        {
            case DIRECTIONAL_LIGHT:
                result = do_directional_light(light, mat, V, P, N);
                break;

            case POINT_LIGHT:
                result = do_point_light(light, mat, V, P, N);
                break;
        }
        lit.diffuse += result.diffuse;
        lit.specular += result.specular;
    }

    vec3 ambient = mat.ambient_color.rgb * mat.global_ambient.rgb;
    vec3 diffuse = mat.diffuse_color.rgb * lit.diffuse;
    vec3 specular = mat.specular_color.rgb * lit.specular;

    out_color = vec4(diffuse + specular, 1.0);
}