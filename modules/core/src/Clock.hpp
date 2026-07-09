#pragma once
#include <cstdint>

namespace engine
{
    // Private (src-only) monotonic clock helper shared by FrameTimer and the pacers
    // that need raw nanosecond timestamps. Not part of the public API.
    int64_t NowNs();
}
