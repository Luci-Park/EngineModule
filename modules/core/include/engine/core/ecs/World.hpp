#pragma once
#include <engine/core/core_export.h>
#include <engine/core/ecs/Column.hpp>
#include <engine/core/ecs/Entity.hpp>
#include <engine/core/ecs/EntityAllocator.hpp>
#include <engine/core/ecs/Mut.hpp>
#include <engine/core/ecs/SparseStorage.hpp>
#include <engine/core/ecs/StorageKind.hpp>
#include <engine/core/ecs/Table.hpp>
#include <engine/core/ecs/TypeId.hpp>
#include <engine/core/log/Assert.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace engine
{
    // entity index = archetype + row within that archetype's table
    struct EntityLocation
    {
        uint32_t m_archetypeId = 0;
        uint32_t m_row = 0;
    };

    // main ecs container
    class ENGINE_CORE_API World
    {
    public:
        World();

        // MSVC's dllexport forces instatiation -> explicit delete needed
        World(const World &) = delete;
        World &operator=(const World &) = delete;
        World(World &&) = default;
        World &operator=(World &&) = default;

        Entity Spawn(); // spawn empty entity

        // spawn entity with components
        template <typename... Ts>
        Entity Spawn(Ts... components)
        {
            Entity e = Spawn();
            (AddComponent(e, std::move(components)), ...);
            return e;
        }

        void Despawn(Entity e);

        bool IsAlive(Entity e) const;
        std::size_t EntityCount() const;

        // adding existing components overwrites
        template <typename T>
        void AddComponent(Entity e, T value)
        {
            if (!m_entities.IsAlive(e))
                return;

            const uint32_t tick = m_currentTick;

            if constexpr (ComponentStorageKind<T>::VALUE == StorageKind::SparseSet)
            {
                SparseStorage<T> &storage = GetOrCreateSparseStorage<T>();
                if (storage.Contains(e))
                {
                    *storage.Get(e) = std::move(value);
                    storage.Meta(e)->m_changedTick = tick; // preserve m_addedTick
                }
                else
                {
                    storage.Insert(e, std::move(value), ComponentMeta{tick, tick});
                }
            }
            else
            {
                const EntityLocation loc = m_locations[e.m_index];
                Archetype &srcArchetype = *m_archetypes[loc.m_archetypeId];

                if (Column<T> *existing = srcArchetype.m_table.GetColumn<T>())
                {
                    existing->Get(loc.m_row) = std::move(value);
                    existing->Meta(loc.m_row).m_changedTick = tick; // preserve m_addedTick
                    return;
                }

                const uint32_t seq = TypeIdOf<T>().m_seq;
                const uint32_t dstId = FindOrCreateArchetypeForAdd(loc.m_archetypeId, seq,
                                                                   []() -> std::unique_ptr<IColumn>
                                                                   { return std::make_unique<Column<T>>(); });

                // holding a storage across a modifying step may cause errors
                // needs re-fetching
                Archetype &src = *m_archetypes[loc.m_archetypeId];
                Archetype &dst = *m_archetypes[dstId];

                const Entity displaced = src.m_table.MoveRowTo(loc.m_row, dst.m_table);
                Column<T> *dstColumn = dst.m_table.GetColumn<T>();
                dstColumn->Push(std::move(value), ComponentMeta{tick, tick});

                EntityLocation &newLoc = m_locations[e.m_index];
                newLoc.m_archetypeId = dstId;
                newLoc.m_row = static_cast<uint32_t>(dst.m_table.RowCount() - 1);
                if (!displaced.IsNull())
                    m_locations[displaced.m_index].m_row = loc.m_row;
            }
        }

        // Remove-absent / dead-entity = safe no-op
        template <typename T>
        void RemoveComponent(Entity e)
        {
            if (!m_entities.IsAlive(e))
                return;

            if constexpr (ComponentStorageKind<T>::VALUE == StorageKind::SparseSet)
            {
                if (ISparseStorage *storage = FindSparseStorage(TypeIdOf<T>().m_seq))
                    storage->Remove(e);
            }
            else
            {
                const EntityLocation loc = m_locations[e.m_index];
                if (m_archetypes[loc.m_archetypeId]->m_table.GetColumn<T>() == nullptr)
                    return;

                const uint32_t seq = TypeIdOf<T>().m_seq;
                const uint32_t dstId = FindOrCreateArchetypeForRemove(loc.m_archetypeId, seq);

                // re-fetch after creation
                Archetype &src = *m_archetypes[loc.m_archetypeId];
                Archetype &dst = *m_archetypes[dstId];

                const Entity displaced = src.m_table.MoveRowTo(loc.m_row, dst.m_table);

                EntityLocation &newLoc = m_locations[e.m_index];
                newLoc.m_archetypeId = dstId;
                newLoc.m_row = static_cast<uint32_t>(dst.m_table.RowCount() - 1);
                if (!displaced.IsNull())
                    m_locations[displaced.m_index].m_row = loc.m_row;
            }
        }

        // nullptr if invalid
        template <typename T>
        T *GetComponent(Entity e)
        {
            if (!m_entities.IsAlive(e))
                return nullptr;

            if constexpr (ComponentStorageKind<T>::VALUE == StorageKind::SparseSet)
            {
                ISparseStorage *storage = FindSparseStorage(TypeIdOf<T>().m_seq);
                return storage == nullptr ? nullptr : static_cast<SparseStorage<T> *>(storage)->Get(e);
            }
            else
            {
                const EntityLocation &loc = m_locations[e.m_index];
                Column<T> *column = m_archetypes[loc.m_archetypeId]->m_table.GetColumn<T>();
                return column == nullptr ? nullptr : &column->Get(loc.m_row);
            }
        }

        template <typename T>
        bool HasComponent(Entity e) const
        {
            if (!m_entities.IsAlive(e))
                return false;

            if constexpr (ComponentStorageKind<T>::VALUE == StorageKind::SparseSet)
            {
                const ISparseStorage *storage = FindSparseStorage(TypeIdOf<T>().m_seq);
                return storage != nullptr && storage->Contains(e);
            }
            else
            {
                const EntityLocation &loc = m_locations[e.m_index];
                return m_archetypes[loc.m_archetypeId]->m_table.HasColumn(TypeIdOf<T>().m_seq);
            }
        }

        uint32_t CurrentTick() const;
        void AdvanceTick();

        // used to track change
        template <typename T>
        std::optional<Mut<T>> GetComponentMut(Entity e)
        {
            if (!m_entities.IsAlive(e))
                return std::nullopt;

            if constexpr (ComponentStorageKind<T>::VALUE == StorageKind::SparseSet)
            {
                ISparseStorage *storage = FindSparseStorage(TypeIdOf<T>().m_seq);
                if (storage == nullptr)
                    return std::nullopt;
                auto *typedStorage = static_cast<SparseStorage<T> *>(storage);
                T *ptr = typedStorage->Get(e);
                if (ptr == nullptr)
                    return std::nullopt;
                return Mut<T>{ptr, &typedStorage->Meta(e)->m_changedTick, m_currentTick};
            }
            else
            {
                const EntityLocation &loc = m_locations[e.m_index];
                Column<T> *column = m_archetypes[loc.m_archetypeId]->m_table.GetColumn<T>();
                if (column == nullptr)
                    return std::nullopt;
                return Mut<T>{&column->Get(loc.m_row), &column->Meta(loc.m_row).m_changedTick, m_currentTick};
            }
        }

        // observation primitive
        template <typename T>
        const ComponentMeta *GetComponentMeta(Entity e)
        {
            if (!m_entities.IsAlive(e))
                return nullptr;

            if constexpr (ComponentStorageKind<T>::VALUE == StorageKind::SparseSet)
            {
                ISparseStorage *storage = FindSparseStorage(TypeIdOf<T>().m_seq);
                if (storage == nullptr)
                    return nullptr;
                return static_cast<SparseStorage<T> *>(storage)->Meta(e);
            }
            else
            {
                const EntityLocation &loc = m_locations[e.m_index];
                Column<T> *column = m_archetypes[loc.m_archetypeId]->m_table.GetColumn<T>();
                return column == nullptr ? nullptr : &column->Meta(loc.m_row);
            }
        }

        // debug invariant checker -- see plan Constraints for what it covers.
        void Validate() const;

    private:
        struct Archetype
        {
            std::vector<uint32_t> m_signature; // sorted seq list of table-component seqs
            Table m_table;
        };

        static constexpr uint32_t EMPTY_ARCHETYPE_ID = 0;
        static constexpr uint32_t INVALID_ARCHETYPE_ID = UINT32_MAX;

        uint32_t RegisterArchetype(std::unique_ptr<Archetype> archetype);
        uint32_t FindArchetypeId(const std::vector<uint32_t> &signature) const;
        uint32_t FindOrCreateArchetypeForAdd(uint32_t srcArchetypeId, uint32_t addSeq,
                                             const std::function<std::unique_ptr<IColumn>()> &makeColumn);
        uint32_t FindOrCreateArchetypeForRemove(uint32_t srcArchetypeId, uint32_t removeSeq);
        uint64_t HashSignature(const std::vector<uint32_t> &signature) const;

        ISparseStorage *FindSparseStorage(uint32_t seq);
        const ISparseStorage *FindSparseStorage(uint32_t seq) const;

        template <typename T>
        SparseStorage<T> &GetOrCreateSparseStorage()
        {
            const uint32_t seq = TypeIdOf<T>().m_seq;
            auto it = m_sparseStorages.find(seq);
            if (it == m_sparseStorages.end())
                it = m_sparseStorages.emplace(seq, std::make_unique<SparseStorage<T>>()).first;
            return static_cast<SparseStorage<T> &>(*it->second);
        }

        EntityAllocator m_entities;
        std::vector<std::unique_ptr<Archetype>> m_archetypes;                           // archetypeId -> Archetype (unique_ptr: stable addresses)
        std::unordered_map<uint64_t, std::vector<uint32_t>> m_signatureIndex;           // sig-hash -> candidate archetypeIds
        std::vector<EntityLocation> m_locations;                                        // entity index -> location
        std::unordered_map<uint32_t, std::unique_ptr<ISparseStorage>> m_sparseStorages; // seq -> storage
        uint32_t m_currentTick = 1;                                                     // 0 reserved -> default ComponentMeta means "never stamped"
    };
}
