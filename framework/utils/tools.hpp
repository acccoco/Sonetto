#pragma once

#include <vector>
#include <fstream>
#include <fmt/format.h>
#include <fmt/color.h>
#include "stb_image.h"


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

// 用于读取 image
class Stbi_8Bit_RAII
{
public:
    /**
     * 每个 pixel 由 N 个 8-bit component 组成，
     *  N = (desired_channels == 0) ? channels_in_flie : desired_channels
     * 注：channels_in_file 表示 image 真实的通道数
     * @param flip_vertical 是否进行 vertical flip。默认情况第一个像素为 top-left，
     *  如果进行 vertical flip，那么第一个像素是 bottom-left
     */
    explicit Stbi_8Bit_RAII(const std::string& file_path, int desired_channels = 0, bool flip_vertical = false);
    ~Stbi_8Bit_RAII();


public:
    Prop<int, Stbi_8Bit_RAII>      width{0};
    Prop<int, Stbi_8Bit_RAII>      height{0};
    Prop<int, Stbi_8Bit_RAII>      channels_in_file{0};

    stbi_uc* data = nullptr;
};


/**
 * 从文件读取内容
 */
std::vector<char> read_file(const std::string& filename);
}    // namespace Hiss


// vector 中是否存在某个元素
template<typename T>
inline bool exist(const std::vector<T>& arr, const T& value)
{
    return std::find(arr.begin(), arr.end(), value) != arr.end();
}


inline std::string fmt_blue(const std::string& str)
{
    return fmt::format(fmt::fg(fmt::terminal_color::blue) | fmt::emphasis::bold, str);
}