#include "core/engine.hpp"
#include "proj_config.hpp"
#include "func/texture.hpp"
#include "func/pipeline_template.hpp"
#include "application.hpp"
#include "vk_config.hpp"
#include "utils/rand.hpp"
#include "run.hpp"

#include "./particle.hpp"
#include "./graphics.hpp"
#include "./nbody.hpp"
#include "./surface.hpp"


namespace ParticleCompute
{


/**
 * 执行顺序：(graphics, compute)+
 * 由于使用的是同一个 queue，因此 graphics 和 compute 之间只需要 pipeline barrier 做同步即可
 */
class App : public Hiss::IApplication
{
public:
    explicit App(Hiss::Engine& engine)
        : Hiss::IApplication(engine)
    {}

    ~App() override = default;

    bool USE_SURFACE = true;


    Graphics* graphics{};
    NBody*    nbody{};
    Surface*  surface{};

#pragma region 特殊的接口
public:
    void prepare() override
    {
        nbody = new NBody(engine);
        nbody->prepare();

        surface = new Surface(engine);
        surface->prepare();

        Hiss::Buffer* storage_buffer;
        uint32_t       num_particles;
        if (USE_SURFACE)
        {
            storage_buffer = surface->storage_buffer;
            num_particles  = surface->num_particles();
        }
        else
        {
            storage_buffer = nbody->storage_buffer;
            num_particles  = nbody->num_particles;
        }

        graphics = new Graphics(engine, storage_buffer, num_particles);
        graphics->prepare();
    };


    void resize() override { graphics->resize(); }


    void update() noexcept override
    {
        engine.frame_manager().acquire_frame();

        if (USE_SURFACE)
            surface->update();
        else
            nbody->update();

        graphics->update();
        engine.frame_manager().submit_frame();
    }


    void clean() override
    {
        spdlog::info("clean");

        graphics->clean();
        nbody->clean();
        surface->clean();
        delete graphics;
        delete nbody;
        delete surface;
    }


#pragma endregion
};
}    // namespace ParticleCompute

RUN(ParticleCompute::App)
