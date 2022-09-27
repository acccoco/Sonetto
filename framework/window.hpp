#pragma once
#include "vk/vk_common.hpp"


namespace Hiss
{
class Window
{

public:
    Window(const std::string& title, int width, int height);
    ~Window();


    GLFWwindow*        window() { return _window; }
    [[nodiscard]] bool has_resized() const { return _user_data.resized; }
    void               clear_resized_state() { _user_data.resized = false; }

    /// 等待退出最小化模式
    void wait_exit_minimized();

    [[nodiscard]] vk::SurfaceKHR create_surface(vk::Instance instance) const;
    [[nodiscard]] vk::Extent2D   get_extent() const;

    [[nodiscard]] bool should_close() const { return glfwWindowShouldClose(this->_window); }

    void poll_event()
    {
        glfwPollEvents();
        if (glfwGetKey(_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(_window, true);
    }


    [[nodiscard]] std::vector<const char*> get_extensions() const;


private:
    static void callback_window_resize(GLFWwindow* window, int width, int height);


    // members =======================================================


private:
    struct UserData
    {
        bool resized = false;
        int  width   = 0;    // window 的尺寸，单位并不是 pixel
        int  height  = 0;
    };


    GLFWwindow* _window    = nullptr;
    UserData    _user_data = {};
    bool        _glfw_init = false;
};
}    // namespace Hiss
