#ifndef SHADER_MACRO
#define SHADER_MACRO


#ifdef HISS_CPP
// 用于内存对齐
#define ALIGN(n) alignas(n)

// 用于定义 namespace
#define NAMESPACE_BEGIN(name)                                                                                          \
    namespace name                                                                                                     \
    {
#define NAMESPACE_END                                                                                                  \
    }                                                                                                                  \
    ;
using namespace glm;

#else    // HISS_CPP

#define ALIGN(n)
#define NAMESPACE_BEGIN(name)
#define NAMESPACE_END


#endif    // HISS_CPP


#endif    // SHADER_MACRO