/**
 * @file CommandBuffer.hpp
 * @author sumin.park
 * @brief Deferred spawn/despawn/add/remove commands, applied at an explicit flush.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include <engine/core/core_export.h>
#include <engine/core/ecs/ComponentMeta.hpp>
#include <engine/core/ecs/Entity.hpp>
#include <engine/core/memory/LinearAllocator.hpp>
#include <engine/core/memory/Memory.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace engine
{
    class World; // complete type needed by the templated record methods; see World.hpp bottom

    enum class CommandKind : uint8_t
    {
        Spawn,
        Despawn,
        Add,
        Remove
    };

    struct Command
    {
        CommandKind m_kind = CommandKind::Spawn;
        uint32_t m_seq = 0;               // Add / Remove only
        Entity m_entity;                  // Spawn: reserved eagerly at record time
        const void *m_payload = nullptr;  // Add only; points into a block, stable until Reset
    };

    class ENGINE_CORE_API CommandBuffer
    {
    public:
        explicit CommandBuffer(World &world, std::size_t blockBytes = DEFAULT_BLOCK_BYTES);

        // holds a World *; a move would leave it pointing at the moved-from husk
        CommandBuffer(const CommandBuffer &) = delete;
        CommandBuffer &operator=(const CommandBuffer &) = delete;
        CommandBuffer(CommandBuffer &&) = delete;
        CommandBuffer &operator=(CommandBuffer &&) = delete;

        Entity Spawn(); // identity reserved now, placed at flush

        template <typename... Ts>
        Entity Spawn(Ts... components)
        {
            Entity e = Spawn();
            (AddComponent(e, std::move(components)), ...);
            return e;
        }

        void Despawn(Entity e);

        template <typename T>
        void AddComponent(Entity e, T value);

        template <typename T>
        void RemoveComponent(Entity e);

        std::size_t PendingCount() const; // tests + debug
        bool IsEmpty() const;

    private:
        friend class World; // FlushCommands reads m_commands, calls Clear

        void *Bump(std::size_t size, std::size_t align);
        void Clear(); // clear commands, Reset blocks, keep block 0

        static constexpr std::size_t DEFAULT_BLOCK_BYTES = 16 * 1024;

        World *m_world = nullptr;
        std::vector<Command> m_commands;
        std::vector<std::unique_ptr<LinearAllocator>> m_blocks; // block 0 always present
        std::size_t m_blockBytes = DEFAULT_BLOCK_BYTES;
    };
}
