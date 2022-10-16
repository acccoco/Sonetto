#pragma once
#include "core/engine.hpp"


namespace Hiss
{
class IApplication
{
public:
    explicit IApplication(Hiss::Engine& engine)
        : engine(engine)
    {}

    virtual ~IApplication() = default;

    virtual void prepare() = 0;
    virtual void resize(){};
    virtual void update() = 0;
    virtual void clean()  = 0;


public:
    Engine& engine;
};


/**
 * 表示一次渲染，主要为了方便引用 engine
 */
struct IPass
{
    explicit IPass(Hiss::Engine& engine)
        : engine(engine)
    {}


    Engine& engine;
};

}    // namespace Hiss