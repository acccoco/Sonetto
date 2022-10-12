#pragma once
#include <filesystem>

#include "utils/tools.hpp"
#include "core/vk_common.hpp"
#include "core/device.hpp"


namespace Hiss
{

// 读取 shader 的 spv 文件，创建 stage，在析构时销毁 shader stage 资源
class ShaderLoader
{
public:
    explicit ShaderLoader(Hiss::Device& device)
        : _device(device)
    {}

    ~ShaderLoader()
    {
        for (auto shader_module: this->_shader_modules)
        {
            _device.vkdevice().destroy(shader_module);
        }
    }

    vk::PipelineShaderStageCreateInfo load(const std::filesystem::path& file, vk::ShaderStageFlagBits stage)
    {
        const std::filesystem::path spv_file_path = file.string() + ".spv";
        if (!std::filesystem::exists(spv_file_path))
            spdlog::error("shader file not exist: {}", spv_file_path.string());

        std::vector<char>          code = read_file(spv_file_path);
        vk::ShaderModuleCreateInfo info = {
                .codeSize = code.size(),    // 单位是字节

                /* vector 容器可以保证转换为 uint32 后仍然是对齐的 */
                .pCode = reinterpret_cast<const uint32_t*>(code.data()),
        };
        vk::ShaderModule shader_module = _device.vkdevice().createShaderModule(info);
        this->_shader_modules.push_back(shader_module);

        return vk::PipelineShaderStageCreateInfo{
                .stage  = stage,
                .module = shader_module,
                .pName  = "main",
        };
    }

private:
    Hiss::Device&                 _device;
    std::vector<vk::ShaderModule> _shader_modules{};
};
}    // namespace Hiss