#pragma once
#include "vk/vk_common.hpp"


namespace Hiss
{

class Instance
{
public:
    Instance(const std::string& app_name, const vk::DebugUtilsMessengerCreateInfoEXT* debug_utils_messenger_info);
    ~Instance();


    vk::Instance                      handle_get() { return _instance; }
    std::optional<vk::PhysicalDevice> gpu_pick(vk::SurfaceKHR surface);

private:
    vk::Instance _instance;
};
}    // namespace Hiss