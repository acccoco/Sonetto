#pragma once
#include "buffer.hpp"
#include "core/device.hpp"


namespace Hiss
{


/**
 * 构造函数参数太多，使用 struct 来包装
 */
struct Image2DCreateInfo
{
    std::string              name;
    vk::Format               format       = {};
    vk::Extent2D             extent       = {};
    vk::ImageUsageFlags      usage        = {};
    vk::ImageTiling          tiling       = vk::ImageTiling::eOptimal;
    vk::SampleCountFlagBits  samples      = vk::SampleCountFlagBits::e1;
    VmaAllocationCreateFlags memory_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    vk::ImageAspectFlags     aspect;
    vk::ImageLayout          init_layout = vk::ImageLayout::eUndefined;
};


/**
 * 不支持 mipmap
 */
class Image2D
{
public:
    struct View
    {
        vk::ImageView             vkview;
        vk::ImageSubresourceRange range;
    };


    /**
     * 创建 image 以及 image view 的资源
     * 需要手动指定 image 的 usage 以及内存的 usage flag
     */
    Image2D(VmaAllocator allocator, Device& device, const Image2DCreateInfo& info);


    /**
     * 外部已经创建好了 image，这里只是简单包装，方便控制
     */
    Image2D(Device& device, vk::Image image, const std::string& name, vk::ImageAspectFlags aspect,
            vk::ImageLayout layout, vk::Format format, vk::Extent2D extent);


    ~Image2D();


    /**
     * 将 buffer 的内容拷贝到当前 image 中
     * @param buffer buffer 的所有内容都拷贝到 image 中
     */
    void copy_buffer_to_image(vk::Buffer buffer);


    /**
     * 创建作为 depth attachment 的 image
     * 初始 layout 为 eDepthStencilAttachmentOptimal
     * 可 sampled
     */
    static std::shared_ptr<Hiss::Image2D>
    create_depth_attach(VmaAllocator allocator, Device& device, vk::Format format, vk::Extent2D extent,
                        const std::string&      name   = "depth attach",
                        vk::SampleCountFlagBits sample = vk::SampleCountFlagBits::e1)
    {
        return std::make_shared<Image2D>(
                allocator, device,
                Image2DCreateInfo{
                        .name    = name,
                        .format  = format,
                        .extent  = extent,
                        .usage   = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
                        .samples = sample,
                        .aspect  = vk::ImageAspectFlagBits::eDepth,
                        .init_layout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                });
    }


    // 各种内存屏障 ====================================================================================================
public:
    /**
     * 立刻执行 layout transfer，会检查连续的相同 layout，减少 gpu 的调用次数
     */
    void transfer_layout_im(vk::ImageLayout new_layout);


    /**
     * 向 command 添加 execution barrier
     */
    void execution_barrier(vk::CommandBuffer command_buffer, const StageAccess& src, const StageAccess& dst);


    /**
     * 向 command buffer 中添加 layout transfer 的指令
     * @param clear 是否清除原数据
     */
    void transfer_layout(vk::CommandBuffer command_buffer, const StageAccess& src, const StageAccess& dst,
                         vk::ImageLayout new_layout, bool clear = false);


    /**
     * 在当前 class 中记录 layout 是非常不可靠的。
     * @details 命令的录制顺序是任意的，如果在 class 中记录 image 的 layout，那么在命令录制时，
     *  image 中记录的 layout 会发生改变。命令的录制顺序和命令实际的执行顺序是不同的
     */
    void memory_barrier(const StageAccess& src, const StageAccess& dst, vk::ImageLayout old_layout,
                        vk::ImageLayout new_layout, vk::CommandBuffer command_buffer);

    // ====================================================================================================


private:
    // 创建 image view
    void _create_view();


public:
    Prop<VkImage, Image2D>              vkimage{};
    Prop<View, Image2D>                 view{};
    Prop<std::string, Image2D>          name{};    // image 的名称，用于 vulkan 的 debug 信息
    Prop<vk::Format, Image2D>           format{};
    Prop<vk::Extent2D, Image2D>         extent{};
    Prop<vk::ImageAspectFlags, Image2D> aspect{};
    vk::ImageView                       vkview() const { return view().vkview; }


private:
    Device& _device;

    VmaAllocator      _allocator  = nullptr;
    VmaAllocation     _allocation = nullptr;
    VmaAllocationInfo _alloc_info{};

    bool is_proxy = false;    // image 来自类的外部，并非在类中创建

    vk::ImageLayout _layout;
};


class StorageImage : public Image2D
{
public:
    StorageImage(VmaAllocator allocator, Device& device, vk::Format format, vk::Extent2D extent,
                 const std::string& name = "")
        : Image2D(allocator, device,
                  Image2DCreateInfo{
                          .name         = name,
                          .format       = format,    // compoenent 是 uint
                          .extent       = extent,
                          .usage        = vk::ImageUsageFlagBits::eStorage,
                          .memory_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                          .aspect       = vk::ImageAspectFlagBits::eColor,    // 表示要访问 RGB 组件
                          .init_layout  = vk::ImageLayout::eGeneral,          // storage image 一定要是这个 layout
                  })
    {}
};


class DepthAttach : public Image2D
{
public:
    DepthAttach(VmaAllocator allocator, Device& device, vk::Format format, vk::Extent2D extent,
                const std::string& name = "", vk::SampleCountFlagBits sample = vk::SampleCountFlagBits::e1)
        : Image2D(allocator, device,
                  Image2DCreateInfo{
                          .name    = name,
                          .format  = format,
                          .extent  = extent,
                          .usage   = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
                          .samples = sample,
                          .memory_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                          .aspect       = vk::ImageAspectFlagBits::eDepth,
                          .init_layout  = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                  })
    {}
};
}    // namespace Hiss