#include "application.hpp"


/**
 * 为了简化：
 *  - 只使用 point 和 方向光
 *  - tile 大小： 16 x 16，每个 workgroup 有 256 个 thread
 *  - 暂不考虑透明物体，后续再扩展
 *  - 阶段
 *      - 预计算：compute shader，每个 tile 的 frustum
 *      - depth pre-pass：普通的 vertex shader，空的 fragment shader
 *      - light culling：compute shader
 *      - final shading：普通的 vertex shadr，特殊的 fragment shader，读取 light list
 */
namespace ForwardPlus
{
class App : public Hiss::IApplication
{
public:
    explicit App(Hiss::Engine& engine)
        : Hiss::IApplication(engine)
    {}
    ~App() override = default;


    struct
    {
        vk::Pipeline depth_prepass;
        vk::Pipeline light_cull;
        vk::Pipeline final_shading;
    } pipelines;
};

}    // namespace ForwardPlus