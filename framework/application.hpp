#pragma once
#include <iostream>
#include <utility>
#include <random>
#include "vk/vk_common.hpp"
#include "window.hpp"
#include "utils/timer.hpp"
#include "proj_profile.hpp"


namespace Hiss
{

/**
 * 入口函数：run
 * 调用关系：run -> { prepare, update, resize, clean, wait_idle }
 */
class Application
{
public:
    explicit Application(std::string app_name)
        : _app_name(std::move(app_name))
    {}

    virtual ~Application() = default;

    void run();


protected:
    virtual void prepare();
    virtual void update(double delte_time) noexcept;
    virtual void resize();
    virtual void clean();
    virtual void wait_idle()  = 0;    // 等待 gpu 执行完现有任务
    virtual void next_frame() = 0;


    [[nodiscard]] std::string app_name() const { return _app_name; }


private:
    /// 初始化 logger 与 validation 的 logger
    void logger_init();


    // members =======================================================


protected:
    std::shared_ptr<spdlog::logger> _logger;
    std::shared_ptr<spdlog::logger> _validation_logger;
    Hiss::Window*                   _window = {nullptr};

    Timer _timer{};

private:
    const std::string _app_name;
};

}    // namespace Hiss


template<typename app_t>
int runner()
{
    try
    {
        app_t app;
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

#define APP_RUN(app_t)                                                                                                 \
    int main(int argc, char** argv)                                                                                    \
    {                                                                                                                  \
        return runner<app_t>();                                                                                        \
    }
