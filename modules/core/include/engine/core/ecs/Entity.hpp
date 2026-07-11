#pragma once
#include <cstdint>

namespace engine
{
    struct Entity
    {
        static constexpr uint32_t NULL_INDEX = UINT32_MAX; // never allocated

        uint32_t m_index = NULL_INDEX; // slot index
        uint32_t m_generation = 0;     // ABA guard, incremented in Free()

        constexpr bool IsNull() const { return m_index == NULL_INDEX; }
        constexpr bool operator==(const Entity &) const = default;
    };

    inline constexpr Entity NULL_ENTITY{};
}
