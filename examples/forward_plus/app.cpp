#include "application.hpp"


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