const uint LOCAL_LIGHT_COUNT = 1024u;    // tile 内最多可以存放多少 light
const uint TILE_SIZE = 16u;


/* 平面的参数 */
struct Plane
{
    vec3 N;    // 法向量
    float d;    // 原点到平面的有向距离
};


/* 球的定义 */
struct Sphere
{
    vec3 c;    // 球心的坐标
    float r;    // 半径
};


/* frustum 的 4 个侧面，面朝向内部 */
struct Frustum
{
    Plane left;
    Plane right;
    Plane top;
    Plane bottom;
};


/**
 * 球是否完全在平面的背面
 */
bool sphere_behind_plane(Sphere sphere, Plane plane)
{
    return plane.d - dot(plane.N, sphere.c) > sphere.r;
}


/**
 * 球是否与 frustum 相交
 * 如果球完全位于 frustum 某个面的背面，那么就没有相交
 * frustum 的面朝向内部
 * @param near, far 是正数
 */
bool sphere_inter_frustum(Sphere sphere, Frustum frustum, float near, float far)
{
    bool res = true;

    // view space 中，摄像机朝向 -z，因此物体的 z 坐标都是 负数
    if (sphere.c.z - sphere.r > -near || sphere.c.z + sphere.r < -far)
    {
        res = false;
    }

    if (sphere_behind_plane(sphere, frustum.left) || sphere_behind_plane(sphere, frustum.right)
    || sphere_behind_plane(sphere, frustum.top) || sphere_behind_plane(sphere, frustum.bottom))
    {
        res = false;
    }

    return res;
}


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