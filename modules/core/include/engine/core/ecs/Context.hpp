/**
 * @file Context.hpp
 * @author sumin.park
 * @brief World-level singleton value, type-erased via a non-intrusive holder.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include <utility>

namespace engine
{
    struct IContext
    {
        virtual ~IContext() = default;
    };

    // non-intrusive: user context types are plain structs, never derived from IContext.
    // contexts are POD-exempt (never migrate, never memcpy'd), so an owning/non-trivial T
    // costs nothing extra here; do not add trivially-copyable or nothrow-move asserts
    template <typename T>
    struct ContextHolder final : IContext
    {
        T m_value;
        explicit ContextHolder(T value) : m_value(std::move(value)) {}
    };
}
