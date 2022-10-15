#define POINT_LIGHT 0
#define DIRECTIONAL_LIGHT  1
#define SPOT_LIGHT  2


// 光源的定义
struct Light
{
    vec4 pos_world;
    vec4 dir_world;
    vec4 pos_view;
    vec4 dir_view;
    vec4 color;

    float spot_light_angle;     // spot light 的角度啊
    float range;                // 光源的范围
    float intensity;
    uint type;
};


// 光照计算的结果
struct LightingResult
{
    vec3 diffuse;
    vec3 specular;
};


// 材质的定义
struct Material
{
    vec4 global_ambient;    // 全局的环境光

    vec4 ambient_color;
    vec4 emissive_color;
    vec4 diffuse_color;
    vec4 specular_color;


    float opacity;          // 不透明度
    float specular_power;   // 就是 phong 模型中的，可以表示表面的光滑程度

    bool has_ambient_texture;
    bool has_emissive_texture;
    bool has_diffuse_texture;
    bool has_specular_texture;

    vec2 _PADDING_;
};


#ifdef USE_TEX_SLOT     // 使用预先分配的材质位置

layout (set = 0, binding = 0) uniform _mat_0_
{
    Material u_mat;     // uniform 意味着只读
};

layout (set = 0, binding = 1) uniform sampler2D ambient_texture;
layout (set = 0, binding = 2) uniform sampler2D emissive_texture;
layout (set = 0, binding = 3) uniform sampler2D diffuse_texture;
layout (set = 0, binding = 4) uniform sampler2D specular_texture;

#define TEX_SLOT_NUM 4


/**
 * 将 texture 的值读入到 mat 结构体中
 */
void parse_material(inout Material mat, vec2 uv)
{
    if (mat.has_diffuse_texture)
    {
        vec3 diffuse_tex = texture(diffuse_texture, uv).rgb;
        if (mat.ambient_color.rgb != vec3(0))
        {
            mat.diffuse_color.rgb *= diffuse_tex;
        }
        else
        {
            mat.diffuse_color.rgb = diffuse_tex;
        }
    }

    if (mat.has_ambient_texture)
    {
        vec3 ambient_tex = texture(ambient_texture, uv).rgb;
        if (mat.ambient_color.rgb != vec3(0))
        {
            mat.ambient_color.rgb *= ambient_tex;
        }
        else
        {
            mat.ambient_color.rgb = ambient_tex;
        }
    }

    if (mat.has_specular_texture)
    {
        vec3 specular_tex = texture(specular_texture, uv).rgb;
        if (mat.specular_color.rgb != vec3(0.0))
        {
            mat.specular_color.rgb *= specular_tex;
        }
        else
        {
            mat.specular_color.rgb = specular_tex;
        }
    }
}

#endif


/**
 * 计算 specular 的结果
 * 向量都是从物体出发
 */
vec3 do_diffuse(Light light, vec3 L, vec3 N)
{
    float NdotL = max(0.0, dot(N, L));
    return light.color.xyz * NdotL;
}


/**
 * 计算 specular 的结果，不考虑物体的颜色，向量都是从物体出发
 * @param V 观察方向
 * @param L 光照方向
 * @param N 物体的表面法线
 */
vec3 do_specular(Light light, Material mat, vec3 V, vec3 L, vec3 N)
{
    // 反射方向，reflect(I, N) = I - 2.0 * dot(N, I) * N
    vec3 R = normalize(reflect(-L, N));
    float RdotV = max(0.0, dot(R, V));

    // 反正不是 blinn-phong
    return light.color.xyz * pow(RdotV, mat.specular_power);
}



/**
 * 使用方向光进行照明：在 view space 中进行计算，不考虑物体颜色，只进行入射光的累积
 * @param P fragment 在 view space 中的位置
 */
LightingResult do_directional_light(Light light, Material mat, vec3 V, vec3 P, vec3 N)
{
    LightingResult result;

    vec3 L = normalize(-light.dir_view.xyz);

    result.diffuse = do_diffuse(light, L, N) * light.intensity;
    result.specular = do_specular(light, mat, V, L, N) * light.intensity;

    return result;
}



/**
 * 计算点光源的衰减
 */
float do_attenuation(Light light, float dis)
{
    // 光源的有效范围是 [0, range]，在 [0.75range, range] 的范围内衰减
    return 1.f - smoothstep(light.range * 0.75f, light.range, dis);
}


/**
 * 使用点光源进行照明，在 view space 中计算，不考虑物体颜色，只进行入射光的累积
 * @param P 是 fragment 在 view space 中的位置
 */
LightingResult do_point_light(Light light, Material mat, vec3 V, vec3 P, vec3 N)
{
    LightingResult result;

    vec3 L = light.pos_view.xyz - P;
    float distance = length(L);
    L /= distance;      // 正规化

    float attenuation = do_attenuation(light, distance);

    result.diffuse = do_diffuse(light, L, N) * attenuation * light.intensity;
    result.specular = do_specular(light, mat, V, L, N) * attenuation * light.intensity;

    return result;
}