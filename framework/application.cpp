#include "application.hpp"
#include "utils/tools.hpp"


void Hiss::Application::run()
{
    prepare();
    while (!_window->should_close())
    {
        _window->poll_event();

        update(_timer.tick_ms());
        next_frame();
        if (_window->has_resized())
        {
            _logger->info("resize: window");
            resize();
        }
    }
    wait_idle();
    clean();
}


void Hiss::Application::logger_init()
{
    _logger = spdlog::stdout_color_st("logger");
    _logger->set_level(spdlog::level::trace);
    _logger->set_pattern("[%^%L%$] %v");

    _validation_logger = spdlog::stdout_color_st("validation");
    _validation_logger->set_level(spdlog::level::trace);
    _validation_logger->set_pattern("(%^V%$) %v");
}


void Hiss::Application::prepare()
{
    logger_init();
    _logger->info("[Application] prepare");

    _window = new Window(_app_name, WINDOW_WIDTH, WINDOW_HEIGHT);
    _logger->info("[window] init extent: {}, {}", WINDOW_WIDTH, WINDOW_HEIGHT);

    _timer.start();
}


void Hiss::Application::resize()
{
    _logger->info("[Application] reseize");

    /* 如果窗口最小化，先暂停程序 */
    _window->wait_exit_minimized();
    _window->clear_resized_state();
}


void Hiss::Application::clean()
{
    _logger->info("[Application] clean");
    DELETE(_window);
}


void Hiss::Application::update(double delte_time) noexcept {}
