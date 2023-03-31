#version 450
#extension GL_GOOGLE_include_directive : enable
#include "../shader/common.glsl"
#include "../shader/light.glsl"


layout(location = 0) in VertFrag vs;
layout(location = 0) out vec4 out_color;    // sfloat 格式，确保数值范围超过 1

layout(set = 0, binding = 1, std140) uniform _2
{
    Scene u_scene;
};
// 使用预先定义的几个纹理
TEXTURE_BINDING(1, 0)
layout(set = 2, binding = 0, std430) buffer _2_0_
{
    Light b_lights[];
};


void main()
{
    Material mat = u_mat;
    parse_material(mat, vs.uv, diffuse_texture, ambient_texture, specular_texture);

    vec3 V = normalize(0 - vs.pos_view);    // view space 中的观察方向
    vec3 P = vs.pos_view;                   // view space 中的 fragment 的位置
    vec3 N = normalize(vs.normal_view);     // view space 中的法线


    // 遍历所有光源，将光照累计起来
    LightingResult lit = LightingResult(vec3(0), vec3(0));
    for (uint light_idx = 0; light_idx < u_scene.light_num; ++light_idx)
    {
        Light          light  = b_lights[light_idx];
        LightingResult result = LightingResult(vec3(0), vec3(0));
        switch (light.type)
        {
            case DIRECTIONAL_LIGHT: {
                result = do_directional_light(light, mat, V, P, N);
            }
            break;
            case POINT_LIGHT: {
                do_point_light(light, mat, V, P, N);
            }
            break;
        }
        lit.diffuse += result.diffuse;
        lit.specular += result.specular;
    }


    // 使用 blinn phong 模型，进行着色
    vec3 diffuse  = mat.diffuse_color.rgb * lit.diffuse;
    vec3 specular = mat.specular_color.rgb * lit.specular;
    vec3 emission = mat.emissive_color.rgb;
    out_color     = vec4(diffuse + specular + emission, 1.0);
}
