#pragma once
#include <iostream>
#include "application.hpp"
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
        Hiss::Application frame(app_name);
        frame.prepare();
        spdlog::info("framework init ok.");

        // 初始化应用
        app_t app(frame);
        app.prepare();
        spdlog::info("app init ok.");


        while (!frame.should_close())
        {
            if (frame.should_resize())
            {
                frame.wait_idle();
                frame.resize();
                app.resize();
                continue;
            }

            frame.perupdate();
            app.update();
            frame.postupdate();
        }
        frame.wait_idle();
        app.clean();
        frame.clean();
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