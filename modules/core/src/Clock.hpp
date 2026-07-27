/**
 * @file Clock.hpp
 * @author sumin.park
 * @brief Monotonic nanosecond clock, private to the core module.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include <cstdint>

namespace engine
{
    // monotonic; never walks backwards, unlike wall-clock time
    int64_t NowNs();
}
