/**
 * @file Mut.hpp
 * @author sumin.park
 * @brief Write-tracking smart reference that stamps a component's change tick on deref.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include <engine/core/log/Assert.hpp>

#include <cstdint>

namespace engine
{
    // both operator*/operator-> stamp: C++ cannot tell a read from a write through a returned
    // reference, so any access is conservatively a write. const T params and Read() are the opt-outs.
    //
    // MUST NOT include World.hpp; World.hpp includes Mut.hpp, and #pragma once turns that cycle
    // into an include-order-dependent error rather than a working back-include. Hence the guard
    // holds a uint32_t* to World's structural version, never a World*.
    template <typename T>
    struct Mut
    {
        Mut(T *ptr, uint32_t *changedTick, uint32_t currentTick,
            [[maybe_unused]] const uint32_t *structuralVersion,
            [[maybe_unused]] uint32_t capturedVersion)
            : m_ptr(ptr), m_changedTick(changedTick), m_currentTick(currentTick)
#ifndef NDEBUG
              ,
              m_structuralVersion(structuralVersion), m_capturedVersion(capturedVersion)
#endif
        {
        }

        T &operator*()
        {
            CheckAlive();
            *m_changedTick = m_currentTick;
            return *m_ptr;
        }

        T *operator->()
        {
            CheckAlive();
            *m_changedTick = m_currentTick;
            return m_ptr;
        }

        // const T& so misuse can only over-report; for a conditionally-writing system
        const T &Read() const
        {
            CheckAlive();
            return *m_ptr;
        }

    private:
        void CheckAlive() const
        {
#ifndef NDEBUG
            ENGINE_ASSERT(m_structuralVersion != nullptr && *m_structuralVersion == m_capturedVersion,
                          "Mut<T> used after a structural change (Spawn/Despawn/Add/Remove); "
                          "its pointers dangle; re-fetch instead of holding Mut across mutations");
#endif
        }

        T *m_ptr;
        uint32_t *m_changedTick;
        uint32_t m_currentTick;
#ifndef NDEBUG
        const uint32_t *m_structuralVersion;
        uint32_t m_capturedVersion;
#endif
    };
}
