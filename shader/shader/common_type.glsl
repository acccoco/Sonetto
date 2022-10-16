#ifdef HISS_CPP
#define ALIGN(n) alignas(n)
using namespace glm;
#else
#define ALIGN(n)
#endif


/**
 * 场景级别的数据，不会轻易改变
 */
struct Scene
{
    ALIGN(16) mat4 proj_matrix;       // 投影矩阵
    ALIGN(16) mat4 in_proj_matrix;    // 投影矩阵的逆矩阵

    ALIGN(4) uint screen_width;     // 屏幕的宽度，px
    ALIGN(4) uint screen_height;    // 屏幕的高度，px
    ALIGN(4) float near;            // 近平面到摄像机的距离，正数
    ALIGN(4) float far;             // 远平面到摄像机的距离

    ALIGN(4) uint light_num;    // 场景中的光源数量
};


/**
 * 每一帧都可能改变的数据
 */
struct Frame
{
    ALIGN(16) mat4 view_matrix;
};


/**
 * 从 vertex shader 传递到 fragment shader 的数据
 */
struct VertFrag
{
    ALIGN(16) vec3 pos_world;
    ALIGN(16) vec3 pos_view;
    ALIGN(16) vec3 normal_world;
    ALIGN(16) vec3 normal_view;
    ALIGN(16) vec3 color;    // 顶点颜色
    ALIGN(8) vec2 uv;
};