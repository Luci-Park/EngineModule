/**
 * @file World.cpp
 * @author sumin.park
 * @brief World entity lifecycle, archetype find-or-create and debug validation.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#include <engine/core/ecs/World.hpp>

#include <algorithm>
#include <cstring>
#include <string_view>

namespace engine
{
    World::World()
        : m_commands(*this)
    {
        RegisterArchetype(std::make_unique<Archetype>());
    }

    World::~World()
    {
        // table components: walking every table row reaches every placed entity; an unplaced
        // entity holds no table components (FireRemoveHooksForEntity is not called for it here,
        // only its sparse components below), so this is complete for the table half
        for (const auto &archetype : m_archetypes)
        {
            const Table &table = archetype->m_table;
            for (std::size_t row = 0; row < table.RowCount(); ++row)
                FireRemoveHooksForEntity(table.EntityAt(row));
        }

        // sparse components: walk each storage's own dense array directly, not per-entity;
        // O(sum of sparse sizes) instead of O(entity count * sparse type count)
        for (auto &[seq, storage] : m_sparseStorages)
        {
            const ComponentInfo *info = m_components.Find(seq);
            if (info == nullptr)
                continue;
            for (std::size_t dense = 0; dense < storage->Size(); ++dense)
            {
                const Entity e = storage->DenseEntityAt(dense);
                FireOnRemove(*info, e, storage->DataFor(e));
            }
        }

        // contexts torn down last: hooks above may release resources INTO a context (C5),
        // so every context must still be alive while the hook sweep runs
        m_contexts.TearDown();
    }

    Entity World::Spawn()
    {
        ENGINE_ASSERT(!m_inHook, "World::Spawn: hook-initiated structural change is forbidden");
        ENGINE_ASSERT(!IsIterating(), "immediate structural change during query iteration; record into Commands() instead");

        Entity e = m_entities.Allocate();
        if (e.m_index >= m_locations.size())
            m_locations.resize(e.m_index + 1);

        const std::size_t row = m_archetypes[EMPTY_ARCHETYPE_ID]->m_table.AddEntity(e);
        m_locations[e.m_index] = EntityLocation{EMPTY_ARCHETYPE_ID, static_cast<uint32_t>(row)};
        ++m_structuralVersion;
        return e;
    }

    Entity World::ReserveEntity()
    {
        ENGINE_ASSERT(!m_inHook, "World::ReserveEntity: hook-initiated structural change is forbidden");

        Entity e = m_entities.Allocate();
        if (e.m_index >= m_locations.size())
            m_locations.resize(e.m_index + 1);
        m_locations[e.m_index] = EntityLocation{INVALID_ARCHETYPE_ID, 0};
        ++m_unplacedCount;
        return e; // no structural bump: nothing relocated, no live Mut<T> invalidated
    }

    void World::PlaceReservedEntity(Entity e)
    {
        const std::size_t row = m_archetypes[EMPTY_ARCHETYPE_ID]->m_table.AddEntity(e);
        m_locations[e.m_index] = EntityLocation{EMPTY_ARCHETYPE_ID, static_cast<uint32_t>(row)};
        --m_unplacedCount;
        ++m_structuralVersion;
    }

    void World::Despawn(Entity e)
    {
        ENGINE_ASSERT(!m_inHook, "World::Despawn: hook-initiated structural change is forbidden");
        ENGINE_ASSERT(!IsIterating(), "immediate structural change during query iteration; record into Commands() instead");
        if (!m_entities.IsAlive(e))
            return;

        const EntityLocation loc = m_locations[e.m_index];
        const bool unplaced = loc.m_archetypeId == INVALID_ARCHETYPE_ID;

        if (!unplaced)
        {
            FireRemoveHooksForEntity(e); // table components, before SwapRemove; on_remove contract

            Archetype &archetype = *m_archetypes[loc.m_archetypeId];
            const Entity displaced = archetype.m_table.SwapRemove(loc.m_row);
            if (!displaced.IsNull())
                m_locations[displaced.m_index].m_row = loc.m_row;
        }

        // sparse: fire and remove together, one pass; Remove() would re-probe presence itself.
        // unplaced entities can still hold sparse components (sparse self-resolves, unit 10 chunk 4).
        // seqs sorted first: m_sparseStorages is unordered_map bucket order, so on_remove order
        // across two hooked sparse components on one entity would otherwise be nondeterministic
        std::vector<uint32_t> presentSeqs;
        for (auto &[seq, storage] : m_sparseStorages)
        {
            if (storage->DataFor(e) != nullptr)
                presentSeqs.push_back(seq);
        }
        std::sort(presentSeqs.begin(), presentSeqs.end());

        for (uint32_t seq : presentSeqs)
        {
            ISparseStorage &storage = *m_sparseStorages[seq];
            void *data = storage.DataFor(e);
            if (const ComponentInfo *info = m_components.Find(seq))
                FireOnRemove(*info, e, data);
            storage.Remove(e);
        }

        m_entities.Free(e);
        ++m_structuralVersion;
        if (unplaced)
            --m_unplacedCount;
    }

    bool World::IsAlive(Entity e) const { return m_entities.IsAlive(e); }

    std::size_t World::EntityCount() const { return m_entities.AliveCount(); }

    uint32_t World::CurrentTick() const { return m_currentTick; }

    void World::AdvanceTick() { ++m_currentTick; }

    uint32_t World::StructuralVersion() const { return m_structuralVersion; }

    uint64_t World::HashSignature(const std::vector<uint32_t> &signature) const
    {
        const std::string_view bytes(reinterpret_cast<const char *>(signature.data()),
                                     signature.size() * sizeof(uint32_t));
        return static_cast<uint64_t>(Fnv1a32(bytes));
    }

    uint32_t World::FindArchetypeId(const std::vector<uint32_t> &signature) const
    {
        auto it = m_signatureIndex.find(HashSignature(signature));
        if (it != m_signatureIndex.end())
        {
            for (uint32_t candidateId : it->second)
            {
                if (m_archetypes[candidateId]->m_signature == signature)
                    return candidateId;
            }
        }
        return INVALID_ARCHETYPE_ID;
    }

    uint32_t World::RegisterArchetype(std::unique_ptr<Archetype> archetype)
    {
        const uint32_t id = static_cast<uint32_t>(m_archetypes.size());
        const uint64_t hash = HashSignature(archetype->m_signature);
        m_archetypes.push_back(std::move(archetype));
        m_signatureIndex[hash].push_back(id);
        return id;
    }

    uint32_t World::FindOrCreateArchetypeForAdd(uint32_t srcArchetypeId, uint32_t addSeq)
    {
        std::vector<uint32_t> dstSig = m_archetypes[srcArchetypeId]->m_signature;
        dstSig.insert(std::upper_bound(dstSig.begin(), dstSig.end(), addSeq), addSeq); // signature stays sorted for binary_search in Query

        if (const uint32_t found = FindArchetypeId(dstSig); found != INVALID_ARCHETYPE_ID)
            return found;

        auto archetype = std::make_unique<Archetype>();
        archetype->m_signature = dstSig;
        for (const auto &[seq, column] : m_archetypes[srcArchetypeId]->m_table.Columns())
            archetype->m_table.AddColumn(seq, column->CloneEmpty());
        archetype->m_table.AddColumn(addSeq, m_components.Get(addSeq).m_makeColumn());

        return RegisterArchetype(std::move(archetype));
    }

    uint32_t World::FindOrCreateArchetypeForRemove(uint32_t srcArchetypeId, uint32_t removeSeq)
    {
        std::vector<uint32_t> dstSig = m_archetypes[srcArchetypeId]->m_signature;
        dstSig.erase(std::remove(dstSig.begin(), dstSig.end(), removeSeq), dstSig.end());

        if (const uint32_t found = FindArchetypeId(dstSig); found != INVALID_ARCHETYPE_ID)
            return found;

        auto archetype = std::make_unique<Archetype>();
        archetype->m_signature = dstSig;
        for (const auto &[seq, column] : m_archetypes[srcArchetypeId]->m_table.Columns())
        {
            if (seq != removeSeq)
                archetype->m_table.AddColumn(seq, column->CloneEmpty());
        }

        return RegisterArchetype(std::move(archetype));
    }

    void World::FireOnAdd(const ComponentInfo &info, Entity e, void *data)
    {
        if (info.m_onAdd == nullptr)
            return;
        m_inHook = true;
        info.m_onAdd(*this, e, data);
        m_inHook = false;
    }

    void World::FireOnRemove(const ComponentInfo &info, Entity e, void *data)
    {
        if (info.m_onRemove == nullptr)
            return;
        m_inHook = true;
        info.m_onRemove(*this, e, data);
        m_inHook = false;
    }

    // table components only; sparse hooks are fired by the caller, which also owns the
    // per-storage removal that would otherwise need a second pass over m_sparseStorages
    void World::FireRemoveHooksForEntity(Entity e)
    {
        const EntityLocation loc = m_locations[e.m_index];
        if (loc.m_archetypeId == INVALID_ARCHETYPE_ID)
            return; // unplaced: no table components yet

        Archetype &archetype = *m_archetypes[loc.m_archetypeId];
        for (uint32_t seq : archetype.m_signature)
        {
            if (const ComponentInfo *info = m_components.Find(seq))
            {
                IColumn *column = archetype.m_table.FindColumn(seq);
                ENGINE_ASSERT(column != nullptr, "FireRemoveHooksForEntity: signature seq has no matching column");
                FireOnRemove(*info, e, column->DataAt(loc.m_row));
            }
        }
    }

    void World::AddErased(const ComponentInfo &info, Entity e, const void *payload)
    {
        ENGINE_ASSERT(!m_inHook, "World::AddErased: hook-initiated structural change is forbidden");
        ENGINE_ASSERT(!IsIterating(), "immediate structural change during query iteration; record into Commands() instead");
        if (!m_entities.IsAlive(e))
            return;

        // sits above the storage-kind branch so it rejects sparse adds too: a half-formed
        // entity that accepts one kind of component and asserts on the other is the hidden-state
        // divergence section 12's explicit-over-magic rule rejects (unit 10, decision C)
        if (m_locations[e.m_index].m_archetypeId == INVALID_ARCHETYPE_ID)
        {
            ENGINE_ASSERT(false, "World::AddErased: entity is alive but unflushed (unplaced); flush before adding");
            return;
        }

        const uint32_t tick = m_currentTick;

        if (info.m_storage == StorageKind::SparseSet)
        {
            ISparseStorage &storage = GetOrCreateSparseStorage(info);
            if (void *existing = storage.DataFor(e); existing != nullptr)
            {
                // overwrite: on_remove sees the outgoing value, on_add the incoming one, at the
                // same address; m_addedTick is deliberately untouched (decision A)
                FireOnRemove(info, e, existing);
                std::memcpy(existing, payload, info.m_size);
                storage.MetaFor(e)->m_changedTick = tick; // preserve m_addedTick
                FireOnAdd(info, e, existing);
            }
            else
            {
                storage.InsertRaw(e, payload, ComponentMeta{tick, tick});
                ++m_structuralVersion; // new sparse component; invalidates live Mut<T>s
                FireOnAdd(info, e, storage.DataFor(e));
            }
        }
        else
        {
            const EntityLocation loc = m_locations[e.m_index];
            Archetype &srcArchetype = *m_archetypes[loc.m_archetypeId];

            if (IColumn *existing = srcArchetype.m_table.FindColumn(info.m_seq))
            {
                void *data = existing->DataAt(loc.m_row);
                FireOnRemove(info, e, data); // overwrite pair, see the sparse branch above
                std::memcpy(data, payload, info.m_size);
                existing->MetaAt(loc.m_row).m_changedTick = tick; // preserve m_addedTick
                FireOnAdd(info, e, data);
                // overwrite in place; no structural change, no further branch to fall into
            }
            else
            {
                const uint32_t dstId = FindOrCreateArchetypeForAdd(loc.m_archetypeId, info.m_seq);

                // archetype creation may reallocate m_archetypes -> re-fetch both
                Archetype &src = *m_archetypes[loc.m_archetypeId];
                Archetype &dst = *m_archetypes[dstId];

                const Entity displaced = src.m_table.MoveRowTo(loc.m_row, dst.m_table);
                ++m_structuralVersion; // archetype transition; invalidates live Mut<T>s. fires no hooks: T is new here, every other column just relocates
                IColumn *dstColumn = dst.m_table.FindColumn(info.m_seq);
                dstColumn->PushRaw(payload, ComponentMeta{tick, tick});

                EntityLocation &newLoc = m_locations[e.m_index];
                newLoc.m_archetypeId = dstId;
                newLoc.m_row = static_cast<uint32_t>(dst.m_table.RowCount() - 1);
                if (!displaced.IsNull())
                    m_locations[displaced.m_index].m_row = loc.m_row;

                FireOnAdd(info, e, dstColumn->DataAt(newLoc.m_row));
            }
        }
    }

    void World::RemoveErased(uint32_t seq, Entity e)
    {
        ENGINE_ASSERT(!m_inHook, "World::RemoveErased: hook-initiated structural change is forbidden");
        ENGINE_ASSERT(!IsIterating(), "immediate structural change during query iteration; record into Commands() instead");
        if (!m_entities.IsAlive(e))
            return;

        // absent info means no entity can hold this component: silent no-op, not an assert
        const ComponentInfo *info = m_components.Find(seq);
        if (info == nullptr)
            return;

        if (info->m_storage != StorageKind::SparseSet && m_locations[e.m_index].m_archetypeId == INVALID_ARCHETYPE_ID)
            return; // unplaced: no table components yet, matches the remove-absent contract

        if (info->m_storage == StorageKind::SparseSet)
        {
            if (ISparseStorage *storage = FindSparseStorage(seq))
            {
                // DataFor also proves presence; Remove()'s own Contains() check runs again,
                // but a hook needs the data pointer before the row is gone, so one extra
                // lookup here is unavoidable (see plan 08, chunk 2 notes)
                if (void *data = storage->DataFor(e); data != nullptr)
                {
                    FireOnRemove(*info, e, data);
                    storage->Remove(e);
                    ++m_structuralVersion; // actually removed; invalidates live Mut<T>s
                }
            }
        }
        else
        {
            const EntityLocation loc = m_locations[e.m_index];
            IColumn *column = m_archetypes[loc.m_archetypeId]->m_table.FindColumn(seq);
            if (column == nullptr)
                return;

            FireOnRemove(*info, e, column->DataAt(loc.m_row)); // before MoveRowTo; data still in place

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

    ISparseStorage *World::FindSparseStorage(uint32_t seq)
    {
        auto it = m_sparseStorages.find(seq);
        return it == m_sparseStorages.end() ? nullptr : it->second.get();
    }

    const ISparseStorage *World::FindSparseStorage(uint32_t seq) const
    {
        auto it = m_sparseStorages.find(seq);
        return it == m_sparseStorages.end() ? nullptr : it->second.get();
    }

    void World::FlushCommands()
    {
        for (const Command &cmd : m_commands.m_commands)
        {
            switch (cmd.m_kind)
            {
            case CommandKind::Spawn:
                if (m_entities.IsAlive(cmd.m_entity))
                    PlaceReservedEntity(cmd.m_entity);
                break;
            case CommandKind::Despawn:
                Despawn(cmd.m_entity); // already IsAlive-guarded
                break;
            case CommandKind::Add:
                if (const ComponentInfo *info = m_components.Find(cmd.m_seq))
                    AddErased(*info, cmd.m_entity, cmd.m_payload);
                else
                    ENGINE_ASSERT(false, "World::FlushCommands: add command names an unregistered component seq");
                break;
            case CommandKind::Remove:
                RemoveErased(cmd.m_seq, cmd.m_entity); // Find-and-return-on-absent inside
                break;
            }
        }
        m_commands.Clear();
    }

    void World::Validate() const
    {
#ifndef NDEBUG
        // identity and placement grow in step: Spawn resizes m_locations to cover every
        // allocated slot, so a mismatch means an entity exists with no location entry
        ENGINE_ASSERT(m_locations.size() == m_entities.SlotCount(),
                      "World::Validate: location map does not track the allocator slot count");

        std::size_t totalRows = 0;
        for (uint32_t archetypeId = 0; archetypeId < m_archetypes.size(); ++archetypeId)
        {
            const Table &table = m_archetypes[archetypeId]->m_table;
            table.Validate();
            totalRows += table.RowCount();

            for (std::size_t row = 0; row < table.RowCount(); ++row)
            {
                const Entity e = table.EntityAt(row);
                ENGINE_ASSERT(m_entities.IsAlive(e), "World::Validate: table row holds a dead entity");

                const EntityLocation &loc = m_locations[e.m_index];
                ENGINE_ASSERT(loc.m_archetypeId == archetypeId && loc.m_row == row,
                              "World::Validate: entity location does not match its table row");
            }
        }
        ENGINE_ASSERT(totalRows + m_unplacedCount == EntityCount(),
                      "World::Validate: total archetype rows + unplaced entities != alive entity count");

        for (const auto &[seq, storage] : m_sparseStorages)
        {
            storage->Validate();
            for (std::size_t dense = 0; dense < storage->Size(); ++dense)
                ENGINE_ASSERT(m_entities.IsAlive(storage->DenseEntityAt(dense)),
                              "World::Validate: sparse storage holds a dead entity");
        }
#endif
    }
}
