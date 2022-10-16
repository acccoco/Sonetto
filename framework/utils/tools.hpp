#pragma once

#include <vector>
#include <fstream>
#include <fmt/format.h>
#include <fmt/color.h>
#include "stb_image.h"


#define BITS_CONTAIN(bits_a, bits_b) ((bits_a & bits_b) == bits_b)


/**
 * 整数 round 除法
 */
#define ROUND(a, b) (((a) + (b) -1) / (b))


/**
 * 释放指针内存，并将指针置为空
 */
#define DELETE(ptr)                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        delete ptr;                                                                                                    \
        ptr = nullptr;                                                                                                 \
    } while (false)


/**
 * 只读属性
 */
template<typename value_t, typename frient_t>
class Prop
{
public:
    friend frient_t;
    Prop() = default;
    explicit Prop(const value_t& value)
        : _value(value)
    {}


    // NOTE 引用可以提高效率，const 可以防止被修改
    inline const value_t& operator()() const { return this->_value; }

private:
    inline void operator=(value_t new_value) { this->_value = new_value; }


    value_t _value;
};


namespace Hiss
{

/**
 * 从文件读取内容
 */
inline std::vector<char> read_file(const std::string& filename)
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
}    // namespace Hiss