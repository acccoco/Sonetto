#pragma once


namespace Hiss
{
class IApplication
{
public:
    virtual void prepare() = 0;
    virtual void resize()  = 0;
    virtual void update()  = 0;
    virtual void clean()   = 0;
    virtual ~IApplication() = default;
};

}    // namespace Hiss