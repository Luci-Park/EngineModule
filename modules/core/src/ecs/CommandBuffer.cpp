/**
 * @file CommandBuffer.cpp
 * @author sumin.park
 * @brief Non-templated CommandBuffer members: construction, spawn/despawn record, arena growth.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#include <engine/core/ecs/CommandBuffer.hpp>
#include <engine/core/ecs/World.hpp>

namespace engine
{
    CommandBuffer::CommandBuffer(World &world, std::size_t blockBytes)
        : m_world(&world), m_blockBytes(blockBytes)
    {
        m_blocks.push_back(std::make_unique<LinearAllocator>(m_blockBytes, MemoryTag::Components));
    }

    Entity CommandBuffer::Spawn()
    {
        Entity e = m_world->ReserveEntity();
        m_commands.push_back(Command{CommandKind::Spawn, 0, e, nullptr});
        return e;
    }

    void CommandBuffer::Despawn(Entity e)
    {
        m_commands.push_back(Command{CommandKind::Despawn, 0, e, nullptr});
    }

    std::size_t CommandBuffer::PendingCount() const { return m_commands.size(); }

    bool CommandBuffer::IsEmpty() const { return m_commands.empty(); }

    void *CommandBuffer::Bump(std::size_t size, std::size_t align)
    {
        if (void *p = m_blocks.back()->Allocate(size, align))
            return p;

        const std::size_t next = size > m_blockBytes * 2 ? size : m_blockBytes * 2;
        m_blocks.push_back(std::make_unique<LinearAllocator>(next, MemoryTag::Components));
        m_blockBytes = next;
        return m_blocks.back()->Allocate(size, align); // fresh block sized >= size; cannot fail
    }

    void CommandBuffer::Clear()
    {
        m_commands.clear();
        for (std::size_t i = 1; i < m_blocks.size(); ++i)
            m_blocks[i].reset();
        m_blocks.resize(1);
        m_blocks[0]->Reset();
    }
}
