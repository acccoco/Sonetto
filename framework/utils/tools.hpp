#pragma once

#include <vector>
#include <fstream>
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
 * @tparam t1 值的类型
 * @tparam t2 可访问该属性的类型
 */
template<typename t1, typename t2>
class Prop
{
public:
    friend t2;
    Prop() = default;
    explicit Prop(t1 value)
        : _value(value)
    {}

    [[nodiscard]] t1 get() const
    {
        return this->_value;
    }

private:
    t1 _value;
};


namespace Hiss
{
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

    [[nodiscard]] int width() const
    {
        return _width;
    }
    [[nodiscard]] int height() const
    {
        return _height;
    }
    [[nodiscard]] int channels_in_file() const
    {
        return _channels;
    }
    [[nodiscard]] stbi_uc* data() const
    {
        return _data;
    }


private:
    stbi_uc* _data     = nullptr;
    int      _width    = 0;
    int      _height   = 0;
    int      _channels = 0;    // image 的实际 component 数
};


/**
 * 从文件读取内容
 */
std::vector<char> read_file(const std::string& filename);
}    // namespace Hiss


template<typename T>
inline bool exist(const std::vector<T>& arr, const T& value)
{
    return std::find(arr.begin(), arr.end(), value) != arr.end();
}