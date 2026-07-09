#include <catch2/catch_test_macros.hpp>

#include <engine/core/FrameTimer.hpp>

#include <chrono>
#include <thread>

using engine::FrameTimer;

TEST_CASE("first Tick returns zero dt", "[core][frametimer]")
{
    FrameTimer timer;
    REQUIRE(timer.Tick() == 0.0f);
}

TEST_CASE("Tick measures elapsed time", "[core][frametimer]")
{
    FrameTimer timer;
    timer.Tick();  // seed
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const float dt = timer.Tick();

    // Generous bounds: sleep is coarse, but dt must be positive and in the ballpark.
    REQUIRE(dt > 0.010f);
    REQUIRE(dt < 0.500f);
}

TEST_CASE("history fills and stays within capacity", "[core][frametimer]")
{
    FrameTimer timer;
    REQUIRE(timer.HistoryCount() == 0);

    for (uint32_t i = 0; i < 10; ++i)
    {
        timer.Tick();
    }
    REQUIRE(timer.HistoryCount() == 10);

    // Push well past capacity; count saturates, never exceeds capacity.
    for (uint32_t i = 0; i < FrameTimer::HISTORY_CAPACITY + 50; ++i)
    {
        timer.Tick();
    }
    REQUIRE(timer.HistoryCount() == FrameTimer::HISTORY_CAPACITY);
}

TEST_CASE("stats stay internally consistent", "[core][frametimer]")
{
    FrameTimer timer;
    for (int i = 0; i < 5; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        timer.Tick();
    }

    const auto& s = timer.Stats();
    REQUIRE(s.dt > 0.0f);
    REQUIRE(s.fps > 0.0f);
    REQUIRE(s.minDt <= s.maxDt);
    REQUIRE(s.dt >= s.minDt);
    REQUIRE(s.dt <= s.maxDt);
}
