#include "func/model.hpp"
#include <fmt/format.h>


struct Acc
{
    Acc(int a) { spdlog::info("init Acc with: {}", a); }
};


struct Bi
{
    Bi()
        : acc(2)
    {}

    Acc acc;
};


int main() {}