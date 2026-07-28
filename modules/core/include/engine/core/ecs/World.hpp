/**
 * @file World.hpp
 * @author sumin.park
 * @brief The ECS container: entity lifecycle, archetype management and component access.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include <engine/core/core_export.h>
#include <engine/core/ecs/Column.hpp>
#include <engine/core/ecs/ComponentRegistry.hpp>
#include <engine/core/ecs/ContextRegistry.hpp>
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

    class ENGINE_CORE_API World
    {
    public:
        World();

        // dllexport forces instantiation of implicit copy ops -> C2280 unless spelled out
        World(const World &) = delete;
        World &operator=(const World &) = delete;
        World(World &&) = default;
        World &operator=(World &&) = default;
        ~World(); // fires every live entity's on_remove hooks; must stay a real destructor

        Entity Spawn();

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
            ENGINE_ASSERT(!m_inHook, "World::AddComponent: hook-initiated structural change is forbidden");
            if (!m_entities.IsAlive(e))
                return;

            const ComponentInfo &info = m_components.Register<T>();
            const uint32_t tick = m_currentTick;

            if constexpr (ComponentStorageKind<T>::VALUE == StorageKind::SparseSet)
            {
                SparseStorage<T> &storage = GetOrCreateSparseStorage<T>();
                if (auto entry = storage.Find(e); entry.m_value != nullptr)
                {
                    // overwrite: on_remove sees the outgoing value, on_add the incoming one,
                    // at the same address; m_addedTick is deliberately untouched (decision A)
                    FireOnRemove(info, e, entry.m_value);
                    *entry.m_value = std::move(value);
                    entry.m_meta->m_changedTick = tick; // preserve m_addedTick
                    FireOnAdd(info, e, entry.m_value);
                }
                else
                {
                    storage.Insert(e, std::move(value), ComponentMeta{tick, tick});
                    ++m_structuralVersion; // new sparse component; invalidates live Mut<T>s
                    FireOnAdd(info, e, storage.Get(e));
                }
            }
            else
            {
                const EntityLocation loc = m_locations[e.m_index];
                Archetype &srcArchetype = *m_archetypes[loc.m_archetypeId];

                if (Column<T> *existing = srcArchetype.m_table.GetColumn<T>())
                {
                    T *data = &existing->Get(loc.m_row);
                    FireOnRemove(info, e, data); // overwrite pair, see the sparse branch above
                    *data = std::move(value);
                    existing->Meta(loc.m_row).m_changedTick = tick; // preserve m_addedTick
                    FireOnAdd(info, e, data);
                    return; // overwrite in place; no structural change
                }

                const uint32_t dstId = FindOrCreateArchetypeForAdd(loc.m_archetypeId, info.m_seq);

                // archetype creation may reallocate m_archetypes -> re-fetch both
                Archetype &src = *m_archetypes[loc.m_archetypeId];
                Archetype &dst = *m_archetypes[dstId];

                const Entity displaced = src.m_table.MoveRowTo(loc.m_row, dst.m_table);
                ++m_structuralVersion; // archetype transition; invalidates live Mut<T>s. fires no hooks: T is new here, every other column just relocates
                Column<T> *dstColumn = dst.m_table.GetColumn<T>();
                dstColumn->Push(std::move(value), ComponentMeta{tick, tick});

                EntityLocation &newLoc = m_locations[e.m_index];
                newLoc.m_archetypeId = dstId;
                newLoc.m_row = static_cast<uint32_t>(dst.m_table.RowCount() - 1);
                if (!displaced.IsNull())
                    m_locations[displaced.m_index].m_row = loc.m_row;

                FireOnAdd(info, e, &dstColumn->Get(newLoc.m_row));
            }
        }

        // Remove-absent / dead-entity = safe no-op
        template <typename T>
        void RemoveComponent(Entity e)
        {
            ENGINE_ASSERT(!m_inHook, "World::RemoveComponent: hook-initiated structural change is forbidden");
            if (!m_entities.IsAlive(e))
                return;

            // no Register<T> here: removal only drops an existing column, no record needed; a
            // defensive remove of a never-added type would otherwise trip the frozen assert for nothing
            const uint32_t seq = TypeIdOf<T>().m_seq;

            if constexpr (ComponentStorageKind<T>::VALUE == StorageKind::SparseSet)
            {
                if (ISparseStorage *storage = FindSparseStorage(seq))
                {
                    // DataFor also proves presence; Remove()'s own Contains() check runs again,
                    // but a hook needs the data pointer before the row is gone, so one extra
                    // lookup here is unavoidable (see plan 08, chunk 2 notes)
                    if (void *data = storage->DataFor(e); data != nullptr)
                    {
                        if (const ComponentInfo *info = m_components.Find(seq))
                            FireOnRemove(*info, e, data);
                        storage->Remove(e);
                        ++m_structuralVersion; // actually removed; invalidates live Mut<T>s
                    }
                }
            }
            else
            {
                const EntityLocation loc = m_locations[e.m_index];
                Column<T> *column = m_archetypes[loc.m_archetypeId]->m_table.GetColumn<T>();
                if (column == nullptr)
                    return;

                if (const ComponentInfo *info = m_components.Find(seq))
                    FireOnRemove(*info, e, &column->Get(loc.m_row)); // before MoveRowTo; data still in place

                const uint32_t dstId = FindOrCreateArchetypeForRemove(loc.m_archetypeId, seq);

                // archetype creation may reallocate m_archetypes -> re-fetch both
                Archetype &src = *m_archetypes[loc.m_archetypeId];
                Archetype &dst = *m_archetypes[dstId];

                const Entity displaced = src.m_table.MoveRowTo(loc.m_row, dst.m_table);
                ++m_structuralVersion; // archetype transition; invalidates live Mut<T>s

                EntityLocation &newLoc = m_locations[e.m_index];
                newLoc.m_archetypeId = dstId;
                newLoc.m_row = static_cast<uint32_t>(dst.m_table.RowCount() - 1);
                if (!displaced.IsNull())
                    m_locations[displaced.m_index].m_row = loc.m_row;
            }
        }

        // reads only; Mut<T> via GetComponentMut/Query is the sole write path
        template <typename T>
        const T *GetComponent(Entity e) const
        {
            if (!m_entities.IsAlive(e))
                return nullptr;

            if constexpr (ComponentStorageKind<T>::VALUE == StorageKind::SparseSet)
            {
                const ISparseStorage *storage = FindSparseStorage(TypeIdOf<T>().m_seq);
                return storage == nullptr ? nullptr : static_cast<const SparseStorage<T> *>(storage)->Get(e);
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

        uint32_t StructuralVersion() const;

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
                auto entry = typedStorage->Find(e);
                if (entry.m_value == nullptr)
                    return std::nullopt;
                return Mut<T>{entry.m_value, &entry.m_meta->m_changedTick, m_currentTick,
                              &m_structuralVersion, m_structuralVersion};
            }
            else
            {
                const EntityLocation &loc = m_locations[e.m_index];
                Column<T> *column = m_archetypes[loc.m_archetypeId]->m_table.GetColumn<T>();
                if (column == nullptr)
                    return std::nullopt;
                return Mut<T>{&column->Get(loc.m_row), &column->Meta(loc.m_row).m_changedTick, m_currentTick,
                              &m_structuralVersion, m_structuralVersion};
            }
        }

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

        template <typename T>
        const ComponentInfo &RegisterComponent() { return m_components.Register<T>(); }
        const ComponentInfo *FindComponentInfo(uint32_t seq) const { return m_components.Find(seq); }
        void FreezeComponents() { m_components.Freeze(); }
        bool ComponentsFrozen() const { return m_components.IsFrozen(); }

        template <typename T>
        void SetComponentHooks(void (*onAdd)(World &, Entity, void *) noexcept, void (*onRemove)(World &, Entity, void *) noexcept)
        {
            m_components.SetHooks<T>(onAdd, onRemove);
        }

        // world-level singletons; see ContextRegistry for the Set/Init/Override verb semantics
        template <typename T>
        T &SetContext(T value) { return m_contexts.Set(std::move(value)); }
        template <typename T>
        T &InitContext(T value) { return m_contexts.Init(std::move(value)); }
        template <typename T>
        T &OverrideContext(T value) { return m_contexts.Override(std::move(value)); }
        template <typename T>
        T &GetContext() { return m_contexts.Get<T>(); }
        template <typename T>
        const T &GetContext() const { return m_contexts.Get<T>(); }
        template <typename T>
        T *TryGetContext() { return m_contexts.TryGet<T>(); }
        template <typename T>
        const T *TryGetContext() const { return m_contexts.TryGet<T>(); }
        template <typename T>
        bool HasContext() const { return m_contexts.Has<T>(); }
        void FreezeContexts() { m_contexts.Freeze(); }
        bool ContextsFrozen() const { return m_contexts.IsFrozen(); }

        void Validate() const;

    private:
        template <typename... Ps>
        friend class Query;

        struct Archetype
        {
            std::vector<uint32_t> m_signature; // sorted seq list of table-component seqs
            Table m_table;
        };

        static constexpr uint32_t EMPTY_ARCHETYPE_ID = 0;
        static constexpr uint32_t INVALID_ARCHETYPE_ID = UINT32_MAX;

        uint32_t RegisterArchetype(std::unique_ptr<Archetype> archetype);
        uint32_t FindArchetypeId(const std::vector<uint32_t> &signature) const;
        uint32_t FindOrCreateArchetypeForAdd(uint32_t srcArchetypeId, uint32_t addSeq);
        uint32_t FindOrCreateArchetypeForRemove(uint32_t srcArchetypeId, uint32_t removeSeq);
        uint64_t HashSignature(const std::vector<uint32_t> &signature) const;

        ISparseStorage *FindSparseStorage(uint32_t seq);
        const ISparseStorage *FindSparseStorage(uint32_t seq) const;

        void FireOnAdd(const ComponentInfo &info, Entity e, void *data);
        void FireOnRemove(const ComponentInfo &info, Entity e, void *data);
        void FireRemoveHooksForEntity(Entity e);

        template <typename T>
        SparseStorage<T> &GetOrCreateSparseStorage()
        {
            const ComponentInfo &info = m_components.Register<T>();
            auto it = m_sparseStorages.find(info.m_seq);
            if (it == m_sparseStorages.end())
                it = m_sparseStorages.emplace(info.m_seq, info.m_makeSparseStorage()).first;
            return static_cast<SparseStorage<T> &>(*it->second);
        }

        EntityAllocator m_entities;
        std::vector<std::unique_ptr<Archetype>> m_archetypes;                           // archetypeId -> Archetype (unique_ptr: stable addresses)
        std::unordered_map<uint64_t, std::vector<uint32_t>> m_signatureIndex;           // sig-hash -> candidate archetypeIds
        std::vector<EntityLocation> m_locations;                                        // entity index -> location
        std::unordered_map<uint32_t, std::unique_ptr<ISparseStorage>> m_sparseStorages; // seq -> storage
        ComponentRegistry m_components;
        ContextRegistry m_contexts;
        uint32_t m_currentTick = 1;                                                     // 0 reserved for never stamped
        uint32_t m_structuralVersion = 0;                                               // bumped on any relocation; Mut<T> captures it to detect dangling
        bool m_inHook = false;                                                          // re-entrancy guard: hook-initiated structural change is forbidden
    };
}
