/**
 * @file IAllocator.hpp
 * @author sumin.park
 * @brief Allocator interface shared by the engine's arena types.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include <engine/core/core_export.h>

#include <cstddef>

namespace engine
{
    class ENGINE_CORE_API IAllocator
    {
    public:
        virtual ~IAllocator() = default;

        virtual void *Allocate(std::size_t size, std::size_t align = alignof(std::max_align_t)) = 0;
        virtual void Deallocate(void *ptr) = 0;
    };
}
