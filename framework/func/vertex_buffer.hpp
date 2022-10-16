#pragma once
#include "core/device.hpp"
#include "buffer.hpp"
#include "vertex.hpp"


namespace Hiss
{


class IndexBuffer2 : public Buffer
{
public:
    using element_t = uint32_t;


    // memory flags 表示：DEVICE_LOCAL
    IndexBuffer2(Device& device, VmaAllocator allocator, const std::vector<uint32_t>& indices,
                 const std::string& name = "")
        : Buffer(device, allocator, sizeof(uint32_t) * indices.size(),
                 vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
                 VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, name),
          index_num(indices.size())
    {
        // 创建 stage buffer，向其中写入数据
        StageBuffer stage_buffer(device, allocator, size(), fmt::format("{}-stage-buffer", name));
        stage_buffer.mem_copy(indices.data(), size());


        // 立即向 indices 中写入
        OneTimeCommand command_buffer{device, device.command_pool()};
        command_buffer().copyBuffer(stage_buffer.vkbuffer(), vkbuffer(), {vk::BufferCopy{.size = size()}});
        command_buffer.exec();
    }


public:
    const size_t index_num;
};


template<typename VertexType>
class VertexBuffer2 : public Buffer
{
public:
    // memory flags: DEVICE_LOCAL
    VertexBuffer2(Device& device, VmaAllocator allocator, const std::vector<VertexType>& vertices,
                  const std::string& name = "")
        : Buffer(device, allocator, sizeof(VertexType) * vertices.size(),
                 vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
                 VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, name),
          vertex_num(vertices.size())
    {
        // 创建 stage buffer，向其中写入数据
        StageBuffer stage_buffer(device, allocator, size(), fmt::format("{}-stage-buffer", name));
        stage_buffer.mem_copy(vertices.data(), size());


        // 立即向 vertices bufffer 中写入
        OneTimeCommand command_buffer{device, device.command_pool()};
        command_buffer().copyBuffer(stage_buffer.vkbuffer(), vkbuffer(), {vk::BufferCopy{.size = size()}});
        command_buffer.exec();
    }


public:
    const size_t vertex_num;
};

}    // namespace Hiss