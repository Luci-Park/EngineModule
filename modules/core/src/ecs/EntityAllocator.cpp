#include <engine/core/ecs/EntityAllocator.hpp>
#include <engine/core/log/Assert.hpp>

namespace engine
{
    Entity EntityAllocator::Allocate()
    {
        uint32_t index;
        // use free list first
        if (!m_freeList.empty())
        {
            index = m_freeList.front();
            m_freeList.pop_front();
        }
        else
        {
            index = static_cast<uint32_t>(m_generations.size());
            ENGINE_ASSERT(index != Entity::NULL_INDEX, "EntityAllocator exhausted slot index space");
            m_generations.push_back(0);
        }

        ++m_aliveCount;
        return Entity{index, m_generations[index]};
    }

    void EntityAllocator::Free(Entity entity)
    {
        if (!IsAlive(entity))
            return;

        uint32_t &generation = m_generations[entity.m_index];
        ++generation;
        --m_aliveCount;

        if (generation == RETIRED_GENERATION)
            return;

        m_freeList.push_back(entity.m_index);
    }

    bool EntityAllocator::IsAlive(Entity entity) const
    {
        if (entity.IsNull())
            return false;
        if (entity.m_index >= m_generations.size())
            return false;
        if (entity.m_generation == RETIRED_GENERATION)
            return false;

        return m_generations[entity.m_index] == entity.m_generation;
    }

    std::size_t EntityAllocator::AliveCount() const
    {
        return m_aliveCount;
    }
}
