#pragma once
#include <cstdint>

namespace engine
{
    // struct used for write-tracking
    template <typename T>
    struct Mut
    {
        T *m_ptr;
        uint32_t *m_changedTick;
        uint32_t m_currentTick;

        T &operator*()
        {
            *m_changedTick = m_currentTick;
            return *m_ptr;
        }

        T *operator->()
        {
            *m_changedTick = m_currentTick;
            return m_ptr;
        }
    };
}
