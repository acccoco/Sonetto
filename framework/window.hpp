#pragma once
#include "vk/vk_common.hpp"
#include "utils/tools.hpp"


namespace Hiss
{
class Window
{
public:
    Window(const std::string& title, int width, int height);
    ~Window();

    void on_resize()
    {
        wait_exit_minimized();
        clear_resized_state();
    }


    [[nodiscard]] bool has_resized() const { return _user_data.resized; }


    [[nodiscard]] vk::SurfaceKHR create_surface(vk::Instance instance) const;


    // 获取窗口的大小，单位是 pixel
    [[nodiscard]] vk::Extent2D get_extent() const;

    [[nodiscard]] bool should_close() const { return glfwWindowShouldClose(this->window); }


    // 处理窗口系统的各种事件：鼠标，键盘
    void poll_event() const
    {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);
    }


    // vulkan 所需的 instance extension
    [[nodiscard]] std::vector<const char*> get_extensions() const;


private:
    static void callback_window_resize(GLFWwindow* window, int width, int height);

    void clear_resized_state() { _user_data.resized = false; }

    /// 等待退出最小化模式
    void wait_exit_minimized() const;


    // members =======================================================


public:
    GLFWwindow* window = nullptr;


private:
    struct UserData
    {
        bool resized = false;
        int  width   = 0;    // window 的尺寸，单位并不是 pixel
        int  height  = 0;
    } _user_data = {};


    bool _glfw_init = false;
};
}    // namespace Hiss
