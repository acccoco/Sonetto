#include "window.hpp"


std::vector<const char*> Hiss::Window::get_extensions() const
{
    assert(_glfw_init);

    std::vector<const char*> extension_list;
    uint32_t                 extension_cnt = 0;
    const char**             extensions    = glfwGetRequiredInstanceExtensions(&extension_cnt);
    for (int i = 0; i < extension_cnt; ++i)
        extension_list.push_back(extensions[i]);
    return extension_list;
}


Hiss::Window::Window(const std::string& title, int width, int height)
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    _glfw_init = true;

    window._value     = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    _user_data.width  = width;
    _user_data.height = height;


    /* 设置窗口大小改变的 callback */
    glfwSetWindowUserPointer(window.get(), &_user_data);
    glfwSetFramebufferSizeCallback(window.get(), callback_window_resize);
}


Hiss::Window::~Window()
{
    glfwDestroyWindow(window.get());
    window._value = nullptr;
    glfwTerminate();
}


void Hiss::Window::wait_exit_minimized() const
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(window.get(), &width, &height);
    while (width == 0 || height == 0)
    {
        glfwWaitEvents();
        glfwGetFramebufferSize(window.get(), &width, &height);
    }
}


void Hiss::Window::callback_window_resize(GLFWwindow* window, int width, int height)
{
    auto user_data     = reinterpret_cast<UserData*>(glfwGetWindowUserPointer(window));
    user_data->resized = true;
    user_data->width   = width;
    user_data->height  = height;
}


vk::SurfaceKHR Hiss::Window::create_surface(vk::Instance instance) const
{
    /* 调用 glfw 来创建 window surface，这样可以避免平台相关的细节 */
    VkSurfaceKHR surface_;
    if (glfwCreateWindowSurface(instance, window.get(), nullptr, &surface_) != VK_SUCCESS)
        throw std::runtime_error("failed to generate widnow surface by glfw.");
    return surface_;
}


/**
 * window 的 extent，单位是 pixel
 */
vk::Extent2D Hiss::Window::get_extent() const
{
    int width, height;
    glfwGetFramebufferSize(window.get(), &width, &height);
    return {.width = static_cast<uint32_t>(width), .height = static_cast<uint32_t>(height)};
}
