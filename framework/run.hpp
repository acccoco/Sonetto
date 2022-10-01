#pragma once
#include <iostream>
#include "engine.hpp"
#include "proj_config.hpp"
#include <spdlog/spdlog.h>


inline void init_log()
{
    spdlog::set_pattern(default_log_config.pattern);
    spdlog::set_level(default_log_config.level);
}


/**********
Application 类的格式规范

class App {
public:
    void prepare();
    void resize();
    void update();
    void clean();
};
************/
template<typename app_t>
int run(const std::string& app_name)
{
    init_log();
    try
    {
        // 初始化 framework
        Hiss::Engine engine(app_name);
        engine.prepare();
        spdlog::info("[runner] framework init ok.");

        // 初始化应用
        app_t app(engine);
        app.prepare();
        spdlog::info("[runner] app init ok.");


        while (!engine.should_close())
        {
            engine.poll_event();
            if (engine.should_resize())
            {
                engine.wait_idle();
                engine.resize();
                app.resize();
                continue;
            }

            engine.preupdate();
            app.update();
            engine.postupdate();
        }
        engine.wait_idle();
        app.clean();
        engine.clean();
    }
    catch (const std::exception& e)
    {
        std::cerr << "exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}


#define RUN(app_t)                                                                                                     \
    int main()                                                                                                         \
    {                                                                                                                  \
        return run<app_t>(#app_t);                                                                                     \
    }