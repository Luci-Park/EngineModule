/**
 * @file ComponentMeta.hpp
 * @author sumin.park
 * @brief Per-row change-detection ticks stored alongside component data.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include <cstdint>

namespace engine
{
    // change-detection -> set by world on add/overwrite/mutate
    struct ComponentMeta
    {
        uint32_t m_changedTick = 0;
        uint32_t m_addedTick = 0;
    };
}
