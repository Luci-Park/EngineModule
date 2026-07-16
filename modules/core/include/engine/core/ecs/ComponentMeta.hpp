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
