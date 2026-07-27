/**
 * @file Clock.cpp
 * @author sumin.park
 * @brief Monotonic clock source backing frame timing and profiling.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#include "Clock.hpp"

#include <chrono>

namespace engine
{
    int64_t NowNs()
    {
        using namespace std::chrono;
        return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
    }
}
