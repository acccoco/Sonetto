#pragma once
#include "application.hpp"


namespace Hiss
{
/**
 * 光源可视化的 pass
 */
class LightPass : public IPass
{
public:
    explicit LightPass(Engine& engine)
        : IPass(engine)
    {}


    void prepare() {}

    void update() {}


    void clean() {}
};
}    // namespace Hiss