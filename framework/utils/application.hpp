#pragma once
#include "engine/engine.hpp"


namespace Hiss
{
class IApplication
{
public:
    explicit IApplication(Hiss::Engine& engine)
        : engine(engine)
    {}

    virtual ~IApplication() = default;

    virtual void prepare(){};
    virtual void resize(){};
    virtual void update(){};
    virtual void clean(){};


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

    virtual ~IPass() = default;


    Engine& engine;
};

}    // namespace Hiss
