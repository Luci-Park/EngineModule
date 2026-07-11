#pragma once
#include <cstdint>

namespace engine
{
    // change-detection ticks, to be implemented later
    struct ComponentMeta
    {
        uint32_t m_changedTick = 0;
        uint32_t m_addedTick = 0;
    };
}
