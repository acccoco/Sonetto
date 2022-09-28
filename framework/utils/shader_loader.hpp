#pragma once
#include "utils/tools.hpp"
#include "vk/vk_common.hpp"
#include "vk/device.hpp"


namespace Hiss
{
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

    vk::PipelineShaderStageCreateInfo load(const std::string& file, vk::ShaderStageFlagBits stage)
    {
        std::vector<char>          code = read_file(file);
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