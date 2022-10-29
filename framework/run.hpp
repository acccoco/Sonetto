#pragma once
#include <iostream>
#include "engine/engine.hpp"
#include "proj_config.hpp"
#include <spdlog/spdlog.h>
#include "utils/application.hpp"


inline void init_log()
{
    spdlog::set_pattern(default_log_config.pattern);
    spdlog::set_level(default_log_config.level);
}


inline void log_begin_region(const std::string& name)
{
    spdlog::info("");
    spdlog::info("================== {} begin ===========================================================", name);
}


inline void log_end_region(const std::string& name)
{
    spdlog::info("================== {} end =============================================================", name);
    spdlog::info("");
}


extern Hiss::Engine* g_engine;


template<typename app_t>
int run(const std::string& app_name)
{
    init_log();
    try
    {
        // 初始化 framework
        log_begin_region("engine init");
        g_engine = new Hiss::Engine(app_name);
        g_engine->prepare();
        log_end_region("engine init");


        // 初始化应用
        log_begin_region("application init");
        Hiss::IApplication* app = new app_t(*g_engine);
        app->prepare();
        log_end_region("application init");


        // 主循环
        while (!g_engine->should_close())
        {
            g_engine->poll_event();

            if (g_engine->should_resize())
            {
                log_begin_region("on_resize");
                g_engine->wait_idle();
                g_engine->resize();
                app->resize();
                log_end_region("on_resize");
                continue;
            }

            g_engine->preupdate();
            app->update();
            g_engine->postupdate();
        }


        // 退出
        log_begin_region("exit");
        g_engine->wait_idle();
        app->clean();
        delete app;
        g_engine->clean();
        log_end_region("exit");
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