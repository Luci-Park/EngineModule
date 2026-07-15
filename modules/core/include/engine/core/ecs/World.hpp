#pragma once
#include <engine/core/core_export.h>
#include <engine/core/ecs/Column.hpp>
#include <engine/core/ecs/Entity.hpp>
#include <engine/core/ecs/EntityAllocator.hpp>
#include <engine/core/ecs/SparseStorage.hpp>
#include <engine/core/ecs/StorageKind.hpp>
#include <engine/core/ecs/Table.hpp>
#include <engine/core/ecs/TypeId.hpp>
#include <engine/core/log/Assert.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
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

        // set storage according to the ComponentStorageKind
        // adding existing components overwrites
        // no-op if e is dead.
        template <typename T>
        void AddComponent(Entity e, T value)
        {
            if (!m_entities.IsAlive(e))
                return;

            if constexpr (ComponentStorageKind<T>::VALUE == StorageKind::SparseSet)
            {
                GetOrCreateSparseStorage<T>().Insert(e, std::move(value));
            }
            else
            {
                const EntityLocation loc = m_locations[e.m_index]; // copy: POD, stable across the creation step below
                Archetype &srcArchetype = *m_archetypes[loc.m_archetypeId];

                if (Column<T> *existing = srcArchetype.m_table.GetColumn<T>())
                {
                    existing->Get(loc.m_row) = std::move(value);
                    return;
                }

                const uint32_t seq = TypeIdOf<T>().m_seq;
                const uint32_t dstId = FindOrCreateArchetypeForAdd(loc.m_archetypeId, seq,
                                                                   []() -> std::unique_ptr<IColumn>
                                                                   { return std::make_unique<Column<T>>(); });

                // re-fetch after creation -- FindOrCreateArchetypeForAdd may have push_back'd
                // into m_archetypes; never hold an Archetype&/Table& across a step that can
                // create one.
                Archetype &src = *m_archetypes[loc.m_archetypeId];
                Archetype &dst = *m_archetypes[dstId];

                const Entity displaced = src.m_table.MoveRowTo(loc.m_row, dst.m_table);
                Column<T> *dstColumn = dst.m_table.GetColumn<T>();
                dstColumn->Push(std::move(value));

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
                    return; // absent -- no-op

                const uint32_t seq = TypeIdOf<T>().m_seq;
                const uint32_t dstId = FindOrCreateArchetypeForRemove(loc.m_archetypeId, seq);

                // re-fetch after creation -- see AddComponent's hazard note.
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

        // debug invariant checker -- see plan Constraints for what it covers.
        void Validate() const;

    private:
        // archetype signature = SORTED seq list of TABLE-component seqs only (sparse components
        // never appear in a signature, never trigger a transition).
        struct Archetype
        {
            std::vector<uint32_t> m_signature;
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
    };
}
