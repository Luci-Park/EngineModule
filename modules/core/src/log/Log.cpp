#include <engine/core/log/Log.hpp>

#include <engine/core/log/Assert.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <algorithm>
#include <memory>

namespace engine
{
    namespace
    {
        bool s_initialized = false;

        spdlog::level::level_enum ToSpd(LogLevel level)
        {
            switch (level)
            {
            case LogLevel::Trace:
                return spdlog::level::trace;
            case LogLevel::Debug:
                return spdlog::level::debug;
            case LogLevel::Info:
                return spdlog::level::info;
            case LogLevel::Warn:
                return spdlog::level::warn;
            case LogLevel::Error:
                return spdlog::level::err;
            case LogLevel::Critical:
                return spdlog::level::critical;
            case LogLevel::Off:
                return spdlog::level::off;
            }
            return spdlog::level::info;
        }
    }

    void InitLog(const LogConfig &config)
    {
        auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console->set_level(ToSpd(config.consoleLevel));

        auto file = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            config.filePath, config.maxFileBytes, config.maxFiles);
        file->set_level(ToSpd(config.fileLevel));

        auto logger = std::make_shared<spdlog::logger>(
            "engine", spdlog::sinks_init_list{console, file});

        logger->set_level(std::min(ToSpd(config.consoleLevel), ToSpd(config.fileLevel)));
        logger->set_pattern("%^[%T] [%l] %v%$ (%s:%#)");
        logger->flush_on(spdlog::level::warn);

        spdlog::set_default_logger(std::move(logger));
        s_initialized = true;
    }

    void FlushLog()
    {
        spdlog::default_logger_raw()->flush();
    }

    namespace detail
    {
        bool ShouldLog(LogLevel level)
        {
            ENGINE_ASSERT(s_initialized, "log used before InitLog()");
            return spdlog::default_logger_raw()->should_log(ToSpd(level));
        }

        void Log(LogLevel level, const char *file, int line, std::string_view msg)
        {
            // "{}" for pre-formatting
            spdlog::default_logger_raw()->log(
                spdlog::source_loc{file, line, ""}, ToSpd(level), "{}", msg);
        }

        void LogAssert(const char *file, int line, const char *expr, std::string_view msg)
        {
            auto *logger = spdlog::default_logger_raw();
            if (msg.empty())
            {
                logger->log(spdlog::source_loc{file, line, ""}, spdlog::level::critical,
                            "Assertion failed: ({})", expr);
            }
            else
            {
                logger->log(spdlog::source_loc{file, line, ""}, spdlog::level::critical,
                            "Assertion failed: ({}) {}", expr, msg);
            }
            // explicit flush
            logger->flush();
        }
    }
}
