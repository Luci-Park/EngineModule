#pragma once
#include <engine/core/core_export.h>

#include <cstddef>
#include <format>
#include <string>
#include <string_view>

namespace engine
{
    enum class LogLevel
    {
        Trace = 0,
        Debug,
        Info,
        Warn,
        Error,
        Critical,
        Off,
    };

    struct LogConfig
    {
        std::string filePath = "logs/engine.log";
        LogLevel consoleLevel = LogLevel::Trace;
        LogLevel fileLevel = LogLevel::Trace;
        std::size_t maxFileBytes = 5 * 1024 * 1024;
        std::size_t maxFiles = 3;
    };

    // install/replace logger
    ENGINE_CORE_API void InitLog(const LogConfig &config = {});

    // INFO/DEBUG not auto-flushed
    ENGINE_CORE_API void FlushLog();

    namespace detail
    {
        // check log level
        ENGINE_CORE_API bool ShouldLog(LogLevel level);
        ENGINE_CORE_API void Log(LogLevel level, const char *file, int line, std::string_view msg);
        ENGINE_CORE_API void LogAssert(const char *file, int line, const char *expr, std::string_view msg);
    }
}

#define ENGINE_LOG_LEVEL_TRACE 0
#define ENGINE_LOG_LEVEL_DEBUG 1
#define ENGINE_LOG_LEVEL_INFO 2
#define ENGINE_LOG_LEVEL_WARN 3
#define ENGINE_LOG_LEVEL_ERROR 4
#define ENGINE_LOG_LEVEL_CRITICAL 5
#define ENGINE_LOG_LEVEL_OFF 6

#ifndef ENGINE_LOG_LEVEL
#ifdef NDEBUG
#define ENGINE_LOG_LEVEL ENGINE_LOG_LEVEL_INFO
#else
#define ENGINE_LOG_LEVEL ENGINE_LOG_LEVEL_TRACE
#endif
#endif

#define ENGINE_LOG_IMPL(lvl, ...)                                                            \
    do                                                                                       \
    {                                                                                        \
        if (::engine::detail::ShouldLog(lvl))                                                \
        {                                                                                    \
            ::engine::detail::Log((lvl), __FILE__, __LINE__, ::std::format("" __VA_ARGS__)); \
        }                                                                                    \
    } while (0)

#if ENGINE_LOG_LEVEL <= ENGINE_LOG_LEVEL_TRACE
#define ENGINE_LOG_TRACE(...) ENGINE_LOG_IMPL(::engine::LogLevel::Trace, __VA_ARGS__)
#else
#define ENGINE_LOG_TRACE(...) ((void)0)
#endif

#if ENGINE_LOG_LEVEL <= ENGINE_LOG_LEVEL_DEBUG
#define ENGINE_LOG_DEBUG(...) ENGINE_LOG_IMPL(::engine::LogLevel::Debug, __VA_ARGS__)
#else
#define ENGINE_LOG_DEBUG(...) ((void)0)
#endif

#if ENGINE_LOG_LEVEL <= ENGINE_LOG_LEVEL_INFO
#define ENGINE_LOG_INFO(...) ENGINE_LOG_IMPL(::engine::LogLevel::Info, __VA_ARGS__)
#else
#define ENGINE_LOG_INFO(...) ((void)0)
#endif

#if ENGINE_LOG_LEVEL <= ENGINE_LOG_LEVEL_WARN
#define ENGINE_LOG_WARN(...) ENGINE_LOG_IMPL(::engine::LogLevel::Warn, __VA_ARGS__)
#else
#define ENGINE_LOG_WARN(...) ((void)0)
#endif

#if ENGINE_LOG_LEVEL <= ENGINE_LOG_LEVEL_ERROR
#define ENGINE_LOG_ERROR(...) ENGINE_LOG_IMPL(::engine::LogLevel::Error, __VA_ARGS__)
#else
#define ENGINE_LOG_ERROR(...) ((void)0)
#endif

#if ENGINE_LOG_LEVEL <= ENGINE_LOG_LEVEL_CRITICAL
#define ENGINE_LOG_CRITICAL(...) ENGINE_LOG_IMPL(::engine::LogLevel::Critical, __VA_ARGS__)
#else
#define ENGINE_LOG_CRITICAL(...) ((void)0)
#endif
