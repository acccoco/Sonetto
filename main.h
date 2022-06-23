/**
* 关于 validation layer
* 开启 validation layer 之前需要先启用 VK_EXT_DEBUG_UTILS_EXTENSION_NAME 扩展
* 还需要在 instance 的创建中指定 "VK_LAYER_KHRONOS_validation" layer
* 使用 validation layer 之后，可以支持 debug messenger
*/

#pragma once
#include <vector>
#include <cstdlib>
#include <optional>
#include <iostream>
#include <stdexcept>

#define VK_ENABLE_BETA_EXTENSIONS    // vulkan_beta.h，在 metal 上运行 vulkan，需要这个
#include <vulkan/vulkan.h>
#include <spdlog/spdlog.h>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <GLFW/glfw3.h>


void init_spdlog();


struct PhysicalDeviceInfo {
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures   features;

    // queue family 相关的信息
    std::vector<VkQueueFamilyProperties> queue_families;
    std::optional<uint32_t>              graphics_queue_family_idx;
    std::optional<uint32_t>              present_queue_family_idx;

    // swapchain 相关的信息
    VkSurfaceCapabilitiesKHR        capabilities;
    std::vector<VkSurfaceFormatKHR> format_list;
    std::vector<VkPresentModeKHR>   present_mode_list;    // TODO 三重缓冲，进一步了解

    // extension
    std::vector<VkExtensionProperties> support_ext_list;
};


class Application
{
public:
    void        run();
    static void print_instance_info();
    void        init_window();
    void        init_vulkan();
    void        mainLoop();
    void        cleanup();


private:
    const std::vector<const char *> _instance_layer_list = {
            "VK_LAYER_KHRONOS_validation",    // validation layer
    };

    std::vector<const char *> _device_ext_list = {
            // 这是一个临时的扩展（vulkan_beta.h)，在 metal API 上模拟 vulkan 需要这个扩展
            VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
            // 可以将渲染结果呈现到 window surface 上
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    VkDebugUtilsMessengerCreateInfoEXT _debug_messenger_create_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            // 会触发 callback 的严重级别
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            // 会触发 callback 的信息类型
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = debug_callback,    // 回调函数
            .pUserData       = nullptr,
    };

    GLFWwindow    *_window{};    // 手动释放
    const uint32_t WIDTH  = 800;
    const uint32_t HEIGHT = 600;
    VkSurfaceKHR   _surface;    // 手动释放

    VkInstance               _instance{VK_NULL_HANDLE};    // 手动释放
    VkDebugUtilsMessengerEXT _debug_messenger;             // 手动释放

    VkPhysicalDevice   _physical_device{VK_NULL_HANDLE};    // 跟随 instance 销毁
    PhysicalDeviceInfo _physical_device_info;

    VkDevice _device;            // 手动释放
    VkQueue  _graphics_queue;    // 跟随 device 销毁
    VkQueue  _present_queue;     // 跟随 device 销毁

    VkSwapchainKHR           _swapchain;               // 手动释放
    std::vector<VkImage>     _swapchain_image_list;    // 跟随 swapchain 销毁
    VkFormat                 _swapchain_iamge_format;
    VkExtent2D               _swapchain_extent;
    std::vector<VkImageView> _swapchain_image_view_list;    // 手动释放

    VkSemaphore _image_available_semaphore;    // 用于 GPU 的同步
    VkSemaphore _render_finished_semaphore;
    VkFence     _in_flight_fence;    // 用于 CPU 的同步

    VkCommandPool   _command_pool;
    VkCommandBuffer _command_buffer;

    VkRenderPass               _render_pass;
    VkPipelineLayout           _pipeline_layout;    // uniform 相关，手动释放资源
    VkPipeline                 _graphics_pipeline;
    std::vector<VkFramebuffer> _swapchain_framebuffer_list;


    /**
     * 用于 debug 的回调函数
     * @param message_severity 严重程度 verbose, info, warning, error
     * @param message_type 信息的类型 general, validation, perfomance
     * @param callback_data 信息的关键内容
     * @param user_data 自定义的数据
     * @return 是否应该 abort
     */
    static VKAPI_ATTR VkBool32 VKAPI_CALL
    debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT      message_severity,
                   VkDebugUtilsMessageTypeFlagsEXT             message_type,
                   const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data);

    void                             create_surface();
    void                             create_instance();
    static std::vector<const char *> get_required_ext();
    bool                             check_instance_layers();
    void                             setup_debug_messenger();

    void               pick_physical_device();
    bool               is_physical_device_suitable(const PhysicalDeviceInfo &physical_device_info);
    static void        print_physical_device_info(const PhysicalDeviceInfo &physical_device_info);
    bool               check_physical_device_ext(const PhysicalDeviceInfo &physical_device_info);
    PhysicalDeviceInfo get_physical_device_info(VkPhysicalDevice physical_device);
    void               create_logical_device();

    // swapchain, image views
    VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR> &formats);
    VkPresentModeKHR choose_swap_present_model(const std::vector<VkPresentModeKHR> &present_modes);
    VkExtent2D       choose_swap_extent(const VkSurfaceCapabilitiesKHR &capabilities);
    void             create_swap_chain();
    void             create_image_views();

    // pipeline
    void create_render_pass();
    void create_piplie();

    // draw
    void create_framebuffers();
    void create_command_pool();
    void create_command_buffer();
    void create_sync_objects();
    void record_command_buffer(VkCommandBuffer buffer, uint32_t image_idx);
    void draw_frame();
};