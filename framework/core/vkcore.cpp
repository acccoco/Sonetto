#include "./vkcore.hpp"


/**
 * vulkan 相关函数的 dynamic loader
 * 这是变量定义的位置，vulkan.hpp 会通过 extern 引用这个全局变量
 * 需要初始化三次：
 *   第一次传入 vkGetInstanceProcAddr
 *   第二次传入 instance
 *   第三次传入 device
 */
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE