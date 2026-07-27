/**
 * @file Align.hpp
 * @author sumin.park
 * @brief Power-of-two alignment helper, private to the memory implementation.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include <cstddef>

namespace engine
{
    // align must be a power of two; the mask depends on it
    inline std::size_t AlignUp(std::size_t value, std::size_t align)
    {
        return (value + (align - 1)) & ~(align - 1);
    }
}
