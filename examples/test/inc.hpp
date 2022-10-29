#pragma once
#include <memory>


struct Acc
{
    static constexpr int acc = 3;
    std::unique_ptr<int> a;
};