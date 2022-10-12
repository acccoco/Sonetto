#pragma once
#include <random>


namespace Hiss
{

// 对 cpp 随机数生成器的简单包装
class Rand
{
public:
    float normal_0_1() { return this->_normal_dist(_engine); }

private:
    std::random_device              _rd;
    std::default_random_engine      _engine{_rd()};
    std::normal_distribution<float> _normal_dist{0.f, 1.f};
};

}    // namespace Hiss