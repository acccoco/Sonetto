#include "./buffer.hpp"


void create_buffer(const vk::Device &device, const DeviceInfo &device_info, vk::DeviceSize size,
                   vk::BufferUsageFlags buffer_usage, vk::MemoryPropertyFlags memory_properties,
                   vk::Buffer &buffer, vk::DeviceMemory &buffer_memory)
{
    // 创建 buffer
    buffer = device.createBuffer({
            .size        = size,
            .usage       = buffer_usage,
            .sharingMode = vk::SharingMode::eExclusive,
    });


    // 分配 memory
    vk::MemoryRequirements mem_require = device.getBufferMemoryRequirements(buffer);
    std::optional<uint32_t> mem_type_idx =
            device_info.find_memory_type(mem_require, memory_properties);
    if (!mem_type_idx.has_value())
        throw std::runtime_error("no proper memory type for buffer, didn't allocate buffer.");
    buffer_memory = device.allocateMemory({
            .allocationSize  = mem_require.size,
            .memoryTypeIndex = mem_type_idx.value(),
    });


    // 绑定
    device.bindBufferMemory(buffer, buffer_memory, 0);
}


void create_index_buffer(const vk::Device &device, const DeviceInfo &device_info,
                         const vk::CommandPool &cmd_pool, const vk::Queue &transfer_queue,
                         const std::vector<uint16_t> &indices, vk::Buffer &index_buffer,
                         vk::DeviceMemory &index_memory)
{
    spdlog::get("logger")->info("create index buffer.");

    vk::DeviceSize buffer_size = sizeof(indices[0]) * indices.size();


    // indices data -> stage buffer
    vk::Buffer stage_buffer;
    vk::DeviceMemory stage_buffer_memory;
    create_buffer(device, device_info, buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
                  vk::MemoryPropertyFlagBits::eHostVisible |
                          vk::MemoryPropertyFlagBits::eHostCoherent,
                  stage_buffer, stage_buffer_memory);

    void *data = device.mapMemory(stage_buffer_memory, 0, buffer_size, {});
    std::memcpy(data, indices.data(), (size_t) buffer_size);
    device.unmapMemory(stage_buffer_memory);


    // stage buffer -> index buffer
    create_buffer(device, device_info, buffer_size,
                  vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
                  vk::MemoryPropertyFlagBits::eDeviceLocal, index_buffer, index_memory);
    OneTimeCmdBuffer one_time_cmd_buffer(device, transfer_queue, cmd_pool);
    one_time_cmd_buffer().copyBuffer(stage_buffer, index_buffer,
                                     {vk::BufferCopy{.size = buffer_size}});
    one_time_cmd_buffer.end();


    // free
    device.destroyBuffer(stage_buffer);
    device.freeMemory(stage_buffer_memory);
}


void create_vertex_buffer_(const vk::Device &device, const DeviceInfo &device_info,
                           const vk::CommandPool &cmd_pool, const vk::Queue &transfer_queue,
                           const std::vector<Vertex> &vertices, vk::Buffer &vertex_buffer,
                           vk::DeviceMemory &vertex_memory)
{
    spdlog::get("logger")->info("create vertex buffer.");

    vk::DeviceSize buffer_size = sizeof(vertices[0]) * vertices.size();


    // vertex data -> stage buffer
    vk::Buffer stage_buffer;
    vk::DeviceMemory stage_buffer_memory;
    create_buffer(device, device_info, buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
                  vk::MemoryPropertyFlagBits::eHostVisible |
                          vk::MemoryPropertyFlagBits::eHostCoherent,
                  stage_buffer, stage_buffer_memory);
    // params: device_memory_obj, offset, size, flags
    auto data = device.mapMemory(stage_buffer_memory, 0, buffer_size, {});
    std::memcpy(data, vertices.data(), (size_t) buffer_size);
    device.unmapMemory(stage_buffer_memory);


    // stage buffer -> vertex buffer
    create_buffer(device, device_info, buffer_size,
                  vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
                  vk::MemoryPropertyFlagBits::eDeviceLocal, vertex_buffer, vertex_memory);
    {
        OneTimeCmdBuffer one_time_cmd_buffer(device, transfer_queue, cmd_pool);
        one_time_cmd_buffer().copyBuffer(stage_buffer, vertex_buffer,
                                         {vk::BufferCopy{.size = buffer_size}});
        one_time_cmd_buffer.end();
    }


    // 销毁 stage buffer 以及 memory
    device.destroyBuffer(stage_buffer);
    device.free(stage_buffer_memory);
}


void create_uniform_buffer(const vk::Device &device, const DeviceInfo &device_info,
                           vk::Buffer &uniform_buffer, vk::DeviceMemory &uniform_memory)
{
    spdlog::get("logger")->info("create uniform buffer.");

    create_buffer(device, device_info, sizeof(UniformBufferObject),
                  vk::BufferUsageFlagBits::eUniformBuffer,
                  vk::MemoryPropertyFlagBits::eHostVisible |
                          vk::MemoryPropertyFlagBits::eHostCoherent,
                  uniform_buffer, uniform_memory);
}


std::vector<vk::Framebuffer>
create_framebuffers(const vk::Device &device, const SurfaceInfo &surface_info,
                    const std::vector<vk::ImageView> &swapchain_image_view_list,
                    const vk::RenderPass &render_pass)
{
    spdlog::get("logger")->info("create framebuffers.");
    std::vector<vk::Framebuffer> swapchain_framebuffer_list;
    swapchain_framebuffer_list.resize(swapchain_image_view_list.size());
    for (size_t i = 0; i < swapchain_image_view_list.size(); ++i)
    {
        vk::ImageView attachments[] = {
                swapchain_image_view_list[i],
        };
        vk::FramebufferCreateInfo framebuffer_create_info = {
                .renderPass      = render_pass,
                .attachmentCount = 1,
                .pAttachments    = attachments,
                .width           = surface_info.extent.width,
                .height          = surface_info.extent.height,
                .layers          = 1,
        };

        swapchain_framebuffer_list[i] = device.createFramebuffer(framebuffer_create_info);
    }
    return swapchain_framebuffer_list;
}


vk::DescriptorPool create_descriptor_pool(const vk::Device &device, uint32_t frames_in_flight)
{
    spdlog::get("logger")->info("create descriptor pool.");

    std::vector<vk::DescriptorPoolSize> pool_size = {
            vk::DescriptorPoolSize{
                    .type            = vk::DescriptorType::eUniformBuffer,
                    .descriptorCount = frames_in_flight,
            },
            vk::DescriptorPoolSize{
                    .type            = vk::DescriptorType::eCombinedImageSampler,
                    .descriptorCount = frames_in_flight,
            }};

    return device.createDescriptorPool(vk::DescriptorPoolCreateInfo{
            .maxSets = frames_in_flight,

            // pool 允许每个种类的 descriptor 各有多少个
            .poolSizeCount = (uint32_t) pool_size.size(),
            .pPoolSizes    = pool_size.data(),
    });
}


vk::CommandPool create_command_pool(const vk::Device &device, const DeviceInfo &device_info)
{
    spdlog::get("logger")->info("create command pool.");
    vk::CommandPoolCreateInfo pool_create_info = {
            .flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = device_info.graphics_queue_family_idx.value(),
    };
    return device.createCommandPool(pool_create_info);
}


void update_uniform_memory(const vk::Device &device, const SurfaceInfo &surface_info,
                           const vk::DeviceMemory &uniform_memory)
{
    static auto start_time = std::chrono::high_resolution_clock::now();

    auto cur_time = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(cur_time - start_time)
                         .count();

    UniformBufferObject ubo = {
            .model = glm::rotate(glm::mat4(1.f), time * glm::radians(90.f),
                                 glm::vec3(0.f, 0.f, 1.f)),
            .view = glm::lookAt(glm::vec3(2.f, 2.f, 2.f), glm::vec3(0.f), glm::vec3(0.f, 0.f, 1.f)),
            .proj = glm::perspective(glm::radians(45.f),
                                     (float) surface_info.extent.width /
                                             (float) surface_info.extent.height,
                                     0.1f, 10.f),
    };
    ubo.proj[1][1] *= -1.f;    // OpenGL 和 vulkan 的坐标系差异

    void *data = device.mapMemory(uniform_memory, 0, sizeof(ubo), {});
    std::memcpy(data, &ubo, sizeof(ubo));
    device.unmapMemory(uniform_memory);
}


std::vector<vk::CommandBuffer> create_command_buffer(const vk::Device &device,
                                                     const vk::CommandPool &command_pool,
                                                     uint32_t frames_in_flight)
{
    spdlog::get("logger")->info("create command buffer.");
    vk::CommandBufferAllocateInfo allocate_info = {
            .commandPool        = command_pool,
            .level              = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = static_cast<uint32_t>(frames_in_flight),
    };

    return device.allocateCommandBuffers(allocate_info);
}


void create_image(const vk::Device &device, const DeviceInfo &device_info,
                  const vk::ImageCreateInfo &image_info, const vk::ImageUsageFlags &image_usage,
                  const vk::MemoryPropertyFlags &mem_properties, vk::Image &image,
                  vk::DeviceMemory &memory)
{
    image = device.createImage(image_info);

    vk::MemoryRequirements mem_require = device.getImageMemoryRequirements(image);
    std::optional<uint32_t> mem_type_idx =
            device_info.find_memory_type(mem_require, mem_properties);
    if (!mem_type_idx.has_value())
        throw std::runtime_error("fail to find proper memory type in a physical device.");
    vk::MemoryAllocateInfo mem_alloc_info = {
            .allocationSize  = mem_require.size,
            .memoryTypeIndex = mem_type_idx.value(),
    };
    memory = device.allocateMemory(mem_alloc_info);

    device.bindImageMemory(image, memory, 0);
}


void create_tex_image(const vk::Device &device, const DeviceInfo &device_info,
                      const vk::Queue &trans_queue, const vk::CommandPool &cmd_pool,
                      const std::string &file_path, vk::Image &tex_image,
                      vk::DeviceMemory &tex_memory)
{
    /* read data from texture file */
    int width, height, channels;
    stbi_uc *data = stbi_load(file_path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!data)
        throw std::runtime_error("failed to load texture.");
    vk::DeviceSize image_size = width * height * 4;


    /* texture data -> stage buffer */
    vk::Buffer stage_buffer;
    vk::DeviceMemory stage_memory;
    create_buffer(device, device_info, image_size, vk::BufferUsageFlagBits::eTransferSrc,
                  vk::MemoryPropertyFlagBits::eHostVisible |
                          vk::MemoryPropertyFlagBits::eHostCoherent,
                  stage_buffer, stage_memory);
    void *stage_data = device.mapMemory(stage_memory, 0, image_size, {});
    std::memcpy(stage_data, data, static_cast<size_t>(image_size));
    device.unmapMemory(stage_memory);


    /* create an image and memory */
    vk::ImageCreateInfo image_info = {
            .imageType   = vk::ImageType::e2D,
            .format      = vk::Format::eR8G8B8A8Srgb,    // 和图片文件保持一致
            .extent      = vk::Extent3D{.width  = static_cast<uint32_t>(width),
                                        .height = static_cast<uint32_t>(height),
                                        .depth  = 1},
            .mipLevels   = 1,
            .arrayLayers = 1,
            .samples     = vk::SampleCountFlagBits::e1,
            .tiling      = vk::ImageTiling::eOptimal,
            .usage       = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
            .sharingMode = vk::SharingMode::eExclusive,
            .initialLayout = vk::ImageLayout::eUndefined,
    };
    create_image(device, device_info, image_info,
                 vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                 vk::MemoryPropertyFlagBits::eDeviceLocal, tex_image, tex_memory);


    /* stage buffer -> image */
    trans_image_layout(device, trans_queue, cmd_pool, tex_image, vk::Format::eR8G8B8A8Srgb,
                       vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    buffer_image_copy(device, trans_queue, cmd_pool, stage_buffer, tex_image,
                      static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    trans_image_layout(device, trans_queue, cmd_pool, tex_image, vk::Format::eR8G8B8A8Srgb,
                       vk::ImageLayout::eTransferDstOptimal,
                       vk::ImageLayout::eShaderReadOnlyOptimal);


    // free resource
    stbi_image_free(data);
    device.destroyBuffer(stage_buffer);
    device.freeMemory(stage_memory);
}


void trans_image_layout(const vk::Device &device, const vk::Queue &trans_queue,
                        const vk::CommandPool &cmd_pool, vk::Image &image, const vk::Format &format,
                        const vk::ImageLayout &old_layout, const vk::ImageLayout &new_layout)
{
    OneTimeCmdBuffer cmd(device, trans_queue, cmd_pool);


    vk::PipelineStageFlags src_stage;
    vk::PipelineStageFlags dst_stage;

    // barrier 有三个作用：synchronize, layout 转换, queue family ownership
    vk::ImageMemoryBarrier barrier = {
            // 这两个参数用于转换 image layout
            .oldLayout = old_layout,
            .newLayout = new_layout,

            //  这两个参数用于转换 queue family ownership
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

            .image = image,

            // 图像的哪些区域会受影响
            .subresourceRange =
                    vk::ImageSubresourceRange{
                            .aspectMask     = vk::ImageAspectFlagBits::eColor,
                            .baseMipLevel   = 0,
                            .levelCount     = 1,
                            .baseArrayLayer = 0,
                            .layerCount     = 1,
                    },
    };
    if (old_layout == vk::ImageLayout::eUndefined &&
        new_layout == vk::ImageLayout::eTransferDstOptimal)
    {
        // 注：transfer 是一个 pseudo 的 pipeline stage，用于进行 transfer 的
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        src_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        dst_stage = vk::PipelineStageFlagBits::eTransfer;
    } else if (old_layout == vk::ImageLayout::eTransferDstOptimal &&
               new_layout == vk::ImageLayout::eShaderReadOnlyOptimal)
    {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        src_stage = vk::PipelineStageFlagBits::eTransfer;
        dst_stage = vk::PipelineStageFlagBits::eFragmentShader;
    } else
        throw std::invalid_argument("unsupported layout transition!");

    // 前三个参数：src 和 dst 的 pipeline stage；xxx
    // 后三个参数：三种 barrier 的 list
    cmd().pipelineBarrier(src_stage, dst_stage, {}, {}, {}, {barrier});

    cmd.end();
}


void buffer_image_copy(const vk::Device &device, const vk::Queue &trans_queue,
                       const vk::CommandPool &cmd_pool, vk::Buffer &buffer, vk::Image &image,
                       uint32_t width, uint32_t height)
{
    OneTimeCmdBuffer cmd(device, trans_queue, cmd_pool);

    vk::BufferImageCopy copy_region = {
            // buffer 中 pixel 的起始位置
            .bufferOffset = 0,

            // pixel 是如何布局的。两者是 0 表示 tightly packed
            .bufferRowLength   = 0,
            .bufferImageHeight = 0,

            // image 的哪些区域需要 copy
            .imageSubresource =
                    vk::ImageSubresourceLayers{
                            .aspectMask     = vk::ImageAspectFlagBits::eColor,
                            .mipLevel       = 0,
                            .baseArrayLayer = 0,
                            .layerCount     = 1,
                    },
            .imageOffset = {0, 0, 0},
            .imageExtent = {width, height, 1},
    };

    // 第 3 个参数：image 当前是什么 layout，这里表示适合用于 transfer to
    cmd().copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, {copy_region});

    cmd.end();
}


vk::ImageView img_view_create(const vk::Device &device, const vk::Image &tex_img,
                              const vk::Format &format)
{
    vk::ImageViewCreateInfo view_info = {
            .image    = tex_img,
            .viewType = vk::ImageViewType::e2D,
            .format   = format,
            .subresourceRange =
                    vk::ImageSubresourceRange{
                            .aspectMask     = vk::ImageAspectFlagBits::eColor,
                            .baseMipLevel   = 0,
                            .levelCount     = 1,
                            .baseArrayLayer = 0,
                            .layerCount     = 1,
                    },
    };

    return device.createImageView(view_info);
}


vk::Sampler sampler_create(const vk::Device &device, const DeviceInfo &device_info)
{
    vk::SamplerCreateInfo sampler_info = {
            .magFilter  = vk::Filter::eLinear,
            .minFilter  = vk::Filter::eLinear,
            .mipmapMode = vk::SamplerMipmapMode::eLinear,

            .addressModeU = vk::SamplerAddressMode::eMirroredRepeat,
            .addressModeV = vk::SamplerAddressMode::eMirroredRepeat,
            .addressModeW = vk::SamplerAddressMode::eMirroredRepeat,

            .mipLodBias = 0.f,

            .anisotropyEnable = VK_TRUE,
            .maxAnisotropy    = device_info.physical_device_properties.limits.maxSamplerAnisotropy,

            // 在 PCF shadow map 中会用到
            .compareEnable = VK_FALSE,
            .compareOp     = vk::CompareOp::eAlways,

            .minLod = 0.f,
            .maxLod = 0.f,

            .borderColor             = vk::BorderColor::eIntOpaqueBlack,
            .unnormalizedCoordinates = VK_FALSE,
    };
    return device.createSampler(sampler_info);
}