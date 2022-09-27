#pragma once
#include <chrono>
#include <utility>


namespace Hiss
{

class Timer
{
public:
    /// 距离上一次 tick 过去了多长时间，单位 ms
    double tick_ms()
    {
        auto now      = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double, std::chrono::milliseconds::period>(now - _pre_tick);
        _pre_tick     = now;
        return duration.count();
    }

    /// 开始运行
    void start() { this->_pre_tick = std::chrono::high_resolution_clock::now(); }

private:
    std::chrono::steady_clock::time_point _pre_tick = {};
};

}    // namespace Hiss