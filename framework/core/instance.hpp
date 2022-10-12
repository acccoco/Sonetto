#pragma once
#include "vk_common.hpp"


namespace Hiss
{

class Instance
{
public:
    Instance(const std::string& app_name, const vk::DebugUtilsMessengerCreateInfoEXT* debug_utils_messenger_info);
    ~Instance();


    vk::Instance vkinstance() { return _instance; }
    std::optional<vk::PhysicalDevice> gpu_pick(vk::SurfaceKHR surface);

private:

    // 检查 layers 是否受支持，因为在 validation layer 启用之前没人报错，所以需要手动检查
    static bool check_layers(const std::vector<const char*>& layers);

    vk::Instance _instance;
};
}    // namespace Hiss