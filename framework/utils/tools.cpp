#include "utils/tools.hpp"
#include "stb_image.h"


Hiss::Stbi_8Bit_RAII::Stbi_8Bit_RAII(const std::string& file_path, int desired_channels, bool flip_vertical)
{
    stbi_set_flip_vertically_on_load(flip_vertical);
    _data = stbi_load(file_path.c_str(), &_width, &_height, &_channels, desired_channels);
    if (_data == nullptr)
        throw std::runtime_error("error: load image, file path: " + file_path);
}


Hiss::Stbi_8Bit_RAII::~Stbi_8Bit_RAII()
{
    stbi_image_free(_data);
}


std::vector<char> Hiss::read_file(const std::string& filename)
{
    // ate：从文件末尾开始读取
    // binary：视为二进制文件
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("failed to open file!");

    // 当前指针位于文件末尾，可以方便地获取文件大小
    size_t            file_size = (size_t) file.tellg();
    std::vector<char> buffer(file_size);

    // 从头开始读取
    file.seekg(0);
    file.read(buffer.data(), (std::streamsize) file_size);

    file.close();
    return buffer;
}