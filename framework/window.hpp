#pragma once
#include "vk_common.hpp"


namespace Hiss
{
class Window
{

public:
    Window(const std::string& title, int width, int height);
    ~Window();


    GLFWwindow*                  handle_get() { return _window; }
    [[nodiscard]] bool           has_resized() const { return _user_data.resized; }
    void                         resize_state_clear() { _user_data.resized = false; }
    void                         wait_exit_minimize();
    [[nodiscard]] vk::SurfaceKHR surface_create(vk::Instance instance) const;
    [[nodiscard]] vk::Extent2D   extent_get() const;


    [[nodiscard]] std::vector<const char*> extensions_get() const;


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
