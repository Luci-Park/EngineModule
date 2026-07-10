#pragma once
#include <cstddef>

namespace engine
{
    // round value up to align for memory
    inline std::size_t AlignUp(std::size_t value, std::size_t align)
    {
        return (value + (align - 1)) & ~(align - 1);
    }
}
