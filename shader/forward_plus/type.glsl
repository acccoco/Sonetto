#ifdef HISS_CPP
#define ALIGN(n) alignas(n)
using namespace glm;
#else
#define ALIGN(n)
#endif


const uint LOCAL_LIGHT_COUNT = 512u;    // tile 内最多可以存放多少 light
const uint TILE_SIZE         = 16u;
const uint WORKGROUP_SIZE    = 16u;


/* 平面的参数 */
struct Plane
{
    ALIGN(16) vec3 N;    // 法向量
    ALIGN(4) float d;    // 原点到平面的有向距离
};


/* 球的定义 */
struct Sphere
{
    ALIGN(16) vec3 c;    // 球心的坐标
    ALIGN(4) float r;    // 半径
};


/* frustum 的 4 个侧面，面朝向内部 */
struct Frustum
{
    ALIGN(16) Plane left;
    ALIGN(16) Plane right;
    ALIGN(16) Plane top;
    ALIGN(16) Plane bottom;
    vec4 _padding_;    // 似乎是 BUG，防止最后一个 float 被覆盖
};