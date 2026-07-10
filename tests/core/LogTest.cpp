#include <catch2/catch_test_macros.hpp>

#include <engine/core/log/Log.hpp>
#include <engine/core/log/Assert.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace
{
    std::string ReadFile(const std::filesystem::path &p)
    {
        std::ifstream in(p, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
}

TEST_CASE("log message reaches the file sink", "[core][log]")
{
    const auto path = std::filesystem::temp_directory_path() / "engine_log_test.log";
    std::filesystem::remove(path);

    engine::LogConfig cfg;
    cfg.filePath = path.string();
    engine::InitLog(cfg);

    ENGINE_LOG_INFO("hello {}", 42);
    engine::FlushLog();

    const std::string contents = ReadFile(path);
    REQUIRE(contents.find("hello 42") != std::string::npos);
    REQUIRE(contents.find("[info]") != std::string::npos);
}

TEST_CASE("braces in already-formatted output are not re-parsed", "[core][log]")
{
    const auto path = std::filesystem::temp_directory_path() / "engine_log_braces.log";
    std::filesystem::remove(path);

    engine::LogConfig cfg;
    cfg.filePath = path.string();
    engine::InitLog(cfg);

    // formatted output has literal braces -> backend treats as data, not fmt.
    ENGINE_LOG_WARN("value is {}", "{not a placeholder}");
    engine::FlushLog();

    REQUIRE(ReadFile(path).find("{not a placeholder}") != std::string::npos);
}

TEST_CASE("VERIFY always evaluates its condition", "[core][assert]")
{
    int calls = 0;
    auto probe = [&]()
    { ++calls; return true; };

    ENGINE_VERIFY(probe());

    // cond side effect must survive every build (Debug + Release).
    REQUIRE(calls == 1);
}
