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
    virtual void resize()  = 0;
    virtual void update()  = 0;
    virtual void clean()   = 0;


public:
    Engine& engine;
};

}    // namespace Hiss