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
