#ifdef HISS_CPP
#define ALIGN(n) alignas(n)
using namespace glm;
#else
#define ALIGN(n)
#endif


#define POINT_LIGHT 0
#define DIRECTIONAL_LIGHT 1
#define SPOT_LIGHT 2

// 光源的定义
struct Light    // total size = 96
{
    ALIGN(16) vec4 pos_world;
    ALIGN(16) vec4 dir_world;
    ALIGN(16) vec4 pos_view;
    ALIGN(16) vec4 dir_view;

    // offset = 64

    ALIGN(16) vec4 color;

    // offset = 80

    ALIGN(4) float spot_light_angle;    // spot light 的角度啊
    ALIGN(4) float range;               // 光源的范围
    ALIGN(4) float intensity;
    ALIGN(4) uint type;
};

// 光照计算的结果
struct LightingResult
{
    ALIGN(16) vec3 diffuse;
    ALIGN(16) vec3 specular;
};

// 材质的定义
struct Material    // total size = 112
{
    ALIGN(16) vec4 global_ambient;    // 全局的环境光

    // offset = 16

    ALIGN(16) vec4 ambient_color;
    ALIGN(16) vec4 emissive_color;
    ALIGN(16) vec4 diffuse_color;
    ALIGN(16) vec4 specular_color;

    // offset = 80

    ALIGN(4) float opacity;           // 不透明度
    ALIGN(4) float specular_power;    // 就是 phong 模型中的，可以表示表面的光滑程度

    // offset = 88

    ALIGN(4) uint has_ambient_texture;
    ALIGN(4) uint has_emissive_texture;
    ALIGN(4) uint has_diffuse_texture;
    ALIGN(4) uint has_specular_texture;

    // offset = 104

    ALIGN(8) vec2 _PADDING_;
};
