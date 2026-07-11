#pragma once
#include <engine/core/core_export.h>
#include <engine/core/ecs/Entity.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace engine
{
    namespace detail
    {
        struct EntityAllocatorTestAccess; // test-only slot poke, see EntityTest.cpp
    }

    class ENGINE_CORE_API EntityAllocator
    {
    public:
        Entity Allocate();
        void Free(Entity entity);          // no-op on null/stale/invalid entity
        bool IsAlive(Entity entity) const; // not null/stale/invalid
        std::size_t AliveCount() const;

    private:
        // generation UINT32_MAX = retired -> never used again
        static constexpr uint32_t RETIRED_GENERATION = UINT32_MAX;

        std::vector<uint32_t> m_generations; // current generation of id
        std::deque<uint32_t> m_freeList;     // FIFO -> oldest freed index reused first
        std::size_t m_aliveCount = 0;

        friend struct detail::EntityAllocatorTestAccess;
    };
}
