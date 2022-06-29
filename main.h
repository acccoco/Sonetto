/**
* 关于 validation layer
* 开启 validation layer 之前需要先启用 VK_EXT_DEBUG_UTILS_EXTENSION_NAME 扩展
* 还需要在 instance 的创建中指定 "VK_LAYER_KHRONOS_validation" layer
* 使用 validation layer 之后，可以支持 debug messenger
*/

#pragma once
#include <array>
#include <vector>
#include <cstdlib>
#include <optional>
#include <iostream>
#include <stdexcept>

#define VK_ENABLE_BETA_EXTENSIONS    // vulkan_beta.h，在 metal 上运行 vulkan，需要这个
#include <vulkan/vulkan.h>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define VULKAN_HPP_NO_UNION_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#include <spdlog/spdlog.h>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <GLFW/glfw3.h>


const int MAX_FRAMES_IN_FILGHT = 2;    // 最多允许同时处理多少帧

// 用于查找函数地址的 dispatcher，vulkan hpp 提供了一个默认的
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE;


void init_spdlog();


struct PhysicalDeviceInfo {
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceMemoryProperties memory_properties;

    std::vector<VkQueueFamilyProperties> queue_families;
    std::optional<uint32_t> graphics_queue_family_idx;
    std::optional<uint32_t> present_queue_family_idx;

    // device 支持的，用于创建 swapchain 的 surface 的信息
    VkSurfaceCapabilitiesKHR device_surface_capabilities;
    std::vector<VkSurfaceFormatKHR> device_surface_format_list;
    std::vector<VkPresentModeKHR> device_surface_present_mode;

    // extension
    std::vector<VkExtensionProperties> support_ext_list;
};


struct Vertex {
    glm::vec2 position;
    glm::vec3 color;

    static VkVertexInputBindingDescription get_binding_description()
    {
        return VkVertexInputBindingDescription{
                /// 对应 binding array 的索引。
                /// 因为只用了一个数组为 VAO 填充数据，因此 binding array 长度为 1，需要的索引就是0
                .binding = 0,
                .stride  = sizeof(Vertex),
                // 是实例化的数据还是只是顶点数据
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        };
    }

    /**
     * 第一个属性是 position，第二个属性是 color
     */
    static std::array<VkVertexInputAttributeDescription, 2> get_attribute_descripions()
    {
        return {
                VkVertexInputAttributeDescription{
                        .location = 0,    // 就是 shader 中 in(location=0)
                        .binding  = 0,    // VAO 数据数组的第几个
                        .format   = VK_FORMAT_R32G32_SFLOAT,
                        .offset   = offsetof(Vertex, position),
                },
                VkVertexInputAttributeDescription{
                        .location = 1,
                        .binding  = 0,
                        .format   = VK_FORMAT_R32G32B32_SFLOAT,
                        .offset   = offsetof(Vertex, color),
                },
        };
    }
};

// 三角形的顶点数据
const std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
        {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}},
};

// 三角形的 indices 数据
const std::vector<uint16_t> indices = {
        0, 1, 2, 2, 3, 0,
};


/**
 * 每一帧所需的同步信息：command buffer，semaphore，fence 等
 */
struct FrameSynchroData {
    VkSemaphore image_available_semaphore;    // 表示从 swapchain 中获取的 image 已经可用
    VkSemaphore render_finish_semaphore;      // 表示渲染已经完成
    VkFence in_flight_fence;
    VkCommandBuffer command_buffer;
};


class Application
{
public:
    void run();
    static void print_instance_info();
    void init_window();
    void init_vulkan();
    void mainLoop();
    void cleanup();


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

    GLFWwindow *_window{};    // 手动释放
    const uint32_t WIDTH  = 800;
    const uint32_t HEIGHT = 600;
    VkSurfaceKHR _surface;    // 手动释放

    VkInstance _instance{VK_NULL_HANDLE};         // 手动释放
    VkDebugUtilsMessengerEXT _debug_messenger;    // 手动释放

    VkPhysicalDevice _physical_device{VK_NULL_HANDLE};    // 跟随 instance 销毁
    PhysicalDeviceInfo _physical_device_info;

    VkDevice _device;    // 手动释放
    vk::Device _device_;
    VkQueue _graphics_queue;    // 跟随 device 销毁
    vk::Queue _graphics_queue_;
    VkQueue _present_queue;    // 跟随 device 销毁

    VkSwapchainKHR _swapchain;                     // 手动释放
    std::vector<VkImage> _swapchain_image_list;    // 跟随 swapchain 销毁
    VkFormat _swapchain_iamge_format;
    VkExtent2D _swapchain_extent;
    std::vector<VkImageView> _swapchain_image_view_list;    // 手动释放
    std::vector<FrameSynchroData> _frames;
    uint32_t _current_frame_idx = 0;    // 当前正在绘制哪一帧

    VkBuffer _vertex_buffer;
    VkDeviceMemory _vertex_buffer_memory;
    vk::Buffer _vertex_buffer_;
    vk::DeviceMemory _vertex_buffer_memory_;
    vk::Buffer _index_buffer;
    vk::DeviceMemory _index_buffer_memory;

    VkCommandPool _command_pool;
    VkRenderPass _render_pass;
    VkPipelineLayout _pipeline_layout;    // uniform 相关，手动释放资源
    VkPipeline _graphics_pipeline;
    std::vector<VkFramebuffer> _swapchain_framebuffer_list;
    bool _framebuffer_resized = false;


    void create_surface();
    void create_instance();
    static std::vector<const char *> get_instance_ext();
    bool check_instance_layers();
    void setup_debug_messenger();

    void pick_physical_device();
    bool is_physical_device_suitable(const PhysicalDeviceInfo &physical_device_info);
    static void print_physical_device_info(const PhysicalDeviceInfo &physical_device_info);
    bool check_physical_device_ext(const PhysicalDeviceInfo &physical_device_info);
    PhysicalDeviceInfo get_physical_device_info(VkPhysicalDevice physical_device);
    void create_logical_device();

    // swapchain, image views
    static VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR> &formats);
    static VkPresentModeKHR choose_swap_present_model(const std::vector<VkPresentModeKHR> &present_modes);
    VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR &capabilities);
    void create_swap_chain();
    void recreate_swapchain();
    void clean_swapchain();
    void create_image_views();

    // pipeline
    void create_render_pass();
    void create_pipeline();

    // draw
    void create_vertex_buffer();
    void create_index_buffer();
    void copy_buffer(vk::Buffer src_buffer, vk::Buffer dst_buffer, vk::DeviceSize size);
    /**
     *  创建 buffer，申请 buffer 的内存，并将 buffer 和 memory 绑定在一起
     * @param memory_properties
     * @param [out]buffer
     * @param [out]buffer_memory
     */
    void create_buffer(vk::DeviceSize size, vk::BufferUsageFlags buffer_usage,
                       vk::MemoryPropertyFlags memory_properties, vk::Buffer &buffer, vk::DeviceMemory &buffer_memory);
    void create_vertex_buffer_();
    VkDeviceMemory alloc_buffer_memory(VkBuffer buffer, VkMemoryPropertyFlags properties);
    void create_frame_synchro_data();
    void create_framebuffers();
    void create_command_pool();
    void record_command_buffer(VkCommandBuffer command_buffer, uint32_t image_idx);
    void record_draw_command_(const vk::CommandBuffer &cmd_buffer, uint32_t image_idx);
    void draw_frame();


    /**
     * 用于 debug 的回调函数
     * @param message_severity 严重程度 verbose, info, warning, error
     * @param message_type 信息的类型 general, validation, perfomance
     * @param callback_data 信息的关键内容
     * @param user_data 自定义的数据
     * @return 是否应该 abort
     */
    static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                                                         VkDebugUtilsMessageTypeFlagsEXT message_type,
                                                         const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
                                                         void *user_data);
};