#pragma once
#include <chrono>
#include <utility>
#include "utils/tools.hpp"


namespace Hiss
{


// stopwatch 计时器
class Timer
{
public:
    void tick()
    {
        auto now    = std::chrono::high_resolution_clock::now();
        duration_ms = std::chrono::duration<double, std::chrono::milliseconds::period>(now - _pre_tick).count();
        now_ms      = std::chrono::duration<double, std::chrono::milliseconds::period>(now - _start).count();

        _pre_tick = now;
    }

    /// 开始运行
    void start() { _start = _pre_tick = std::chrono::high_resolution_clock::now(); }

public:
    /// 距离上一次 tick 过去了多长时间，单位 ms
    Prop<double, Timer> duration_ms{};

    Prop<double, Timer> now_ms{};

private:
    std::chrono::steady_clock::time_point _pre_tick = {};
    std::chrono::steady_clock::time_point _start    = {};
};

}    // namespace Hiss