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

    // loose bounds, sleep is coarse.
    REQUIRE(dt > 0.010f);
    REQUIRE(dt < 0.500f);
}

TEST_CASE("history fills and stays within capacity", "[core][frametimer]")
{
    FrameTimer timer;
    REQUIRE(timer.HistoryCount() == 0);

    // first Tick seeds only (no sample) -> N+1 ticks yield N samples.
    timer.Tick();
    REQUIRE(timer.HistoryCount() == 0);

    for (uint32_t i = 0; i < 10; ++i)
    {
        timer.Tick();
    }
    REQUIRE(timer.HistoryCount() == 10);

    // past capacity -> count saturates.
    for (uint32_t i = 0; i < FrameTimer::HISTORY_CAPACITY + 50; ++i)
    {
        timer.Tick();
    }
    REQUIRE(timer.HistoryCount() == FrameTimer::HISTORY_CAPACITY);
}

TEST_CASE("seed frame does not pollute minDt", "[core][frametimer]")
{
    FrameTimer timer;
    timer.Tick();  // seed only

    // real frames -> minDt must reflect a sample, not the seed 0.
    for (int i = 0; i < 4; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        timer.Tick();
    }

    REQUIRE(timer.Stats().minDt > 0.0f);
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
