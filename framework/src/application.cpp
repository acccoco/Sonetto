#include "application.hpp"
#include "tools.hpp"


void Hiss::Application::run()
{
    prepare();
    while (!glfwWindowShouldClose(_window->handle_get()))
    {
        glfwPollEvents();
        if (glfwGetKey(_window->handle_get(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(_window->handle_get(), true);

        /* delta time */
        auto now      = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double, std::chrono::seconds::period>(now - _pre_tick);
        _pre_tick     = now;

        update(duration.count());
        next_frame();
        if (_window->has_resized())
        {
            resize();
            _window->resize_state_clear();
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

    _window = new Window(_app_name, WINDOW_INIT_WIDTH, WINDOW_INIT_HEIGHT);
    _logger->info("[window] init extent: {}, {}", WINDOW_INIT_WIDTH, WINDOW_INIT_HEIGHT);

    _pre_tick = std::chrono::high_resolution_clock::now();
}


void Hiss::Application::resize()
{
    _logger->info("[Application] reseize");

    /* 如果窗口最小化，先暂停程序 */
    _window->wait_exit_minimize();
    _window->resize_state_clear();
}


void Hiss::Application::clean()
{
    _logger->info("[Application] clean");
    DELETE(_window);
}


void Hiss::Application::update(double delte_time) noexcept
{}


float Hiss::Application::rand_norm()
{
    static std::random_device              rd;
    static std::default_random_engine      engine(rd());
    static std::normal_distribution<float> normal_dis(0.f, 1.f);

    return normal_dis(engine);
}