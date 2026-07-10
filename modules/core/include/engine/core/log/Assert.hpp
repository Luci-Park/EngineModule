#pragma once
#include <engine/core/log/Log.hpp>

#include <cstdlib>
#include <format>

#if defined(_MSC_VER)
#define ENGINE_DEBUG_BREAK() __debugbreak()
#elif defined(__clang__) || defined(__GNUC__)
#define ENGINE_DEBUG_BREAK() __builtin_trap()
#else
#define ENGINE_DEBUG_BREAK() (::std::abort()) // unknown: abort, don't fall through into UB
#endif

#define ENGINE_ASSERT_FAIL(cond, ...)                                                              \
    do                                                                                             \
    {                                                                                              \
        if (!(cond))                                                                               \
        {                                                                                          \
            ::engine::detail::LogAssert(__FILE__, __LINE__, #cond, ::std::format("" __VA_ARGS__)); \
            ENGINE_DEBUG_BREAK();                                                                  \
        }                                                                                          \
    } while (0)

#ifdef NDEBUG
#define ENGINE_ASSERT(cond, ...) ((void)0)
#define ENGINE_VERIFY(cond, ...) \
    do                           \
    {                            \
        (void)(cond);            \
    } while (0)
#else
#define ENGINE_ASSERT(cond, ...) ENGINE_ASSERT_FAIL(cond, __VA_ARGS__)
#define ENGINE_VERIFY(cond, ...) ENGINE_ASSERT_FAIL(cond, __VA_ARGS__)
#endif
