#pragma once
#include <random>


namespace Hiss
{

// 对 cpp 随机数生成器的简单包装
class Rand
{
public:
    /**
     * 均值为 0，标准差为 1 的正态分布
     */
    float normal_0_1() { return this->_normal_dist(_engine); }


    /**
     * 范围为 [-1, 1] 的 uniform 分布
     */
    float uniform_np1() { return _uniform_dist(_engine) * 2.f - 1.f; }

    /**
     * 范围为 [0, 1] 的 uniform 分布
     */
    float uniform_0_1() { return _uniform_dist(_engine); }

private:
    // 随机数种子
    std::random_device _rd;

    // 随机数生成算法
    std::default_random_engine _engine{_rd()};

    // 两个参数分别是：均值，标准差
    std::normal_distribution<float> _normal_dist{0.f, 1.f};


    std::uniform_real_distribution<float> _uniform_dist{0.f, 1.f};
};

}    // namespace Hiss