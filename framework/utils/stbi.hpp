#pragma once


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
    explicit Stbi_8Bit_RAII(const std::string& file_path, int desired_channels = 0, bool flip_vertical = false)
    {
        stbi_set_flip_vertically_on_load(flip_vertical);
        data = stbi_load(file_path.c_str(), &width._value, &height._value, &channels_in_file._value, desired_channels);
        if (data == nullptr)
            throw std::runtime_error("error: _load image, file path: " + file_path);
    }


    ~Stbi_8Bit_RAII() { stbi_image_free(data); }


public:
    Prop<int, Stbi_8Bit_RAII> width{0};
    Prop<int, Stbi_8Bit_RAII> height{0};
    Prop<int, Stbi_8Bit_RAII> channels_in_file{0};

    stbi_uc* data = nullptr;
};


}    // namespace Hiss