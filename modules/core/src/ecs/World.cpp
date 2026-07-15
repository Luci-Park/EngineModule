#include <engine/core/ecs/World.hpp>

#include <algorithm>
#include <string_view>

namespace engine
{
    World::World()
    {
        RegisterArchetype(std::make_unique<Archetype>());
    }

    Entity World::Spawn()
    {
        Entity e = m_entities.Allocate();
        if (e.m_index >= m_locations.size())
            m_locations.resize(e.m_index + 1);

        const std::size_t row = m_archetypes[EMPTY_ARCHETYPE_ID]->m_table.AddEntity(e);
        m_locations[e.m_index] = EntityLocation{EMPTY_ARCHETYPE_ID, static_cast<uint32_t>(row)};
        return e;
    }

    void World::Despawn(Entity e)
    {
        if (!m_entities.IsAlive(e))
            return;

        const EntityLocation loc = m_locations[e.m_index];
        Archetype &archetype = *m_archetypes[loc.m_archetypeId];
        const Entity displaced = archetype.m_table.SwapRemove(loc.m_row);
        if (!displaced.IsNull())
            m_locations[displaced.m_index].m_row = loc.m_row;

        for (auto &[seq, storage] : m_sparseStorages)
            storage->Remove(e);

        m_entities.Free(e);
    }

    bool World::IsAlive(Entity e) const { return m_entities.IsAlive(e); }

    std::size_t World::EntityCount() const { return m_entities.AliveCount(); }

    uint64_t World::HashSignature(const std::vector<uint32_t> &signature) const
    {
        // hashing through 32-bit FNV-1a
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

    uint32_t World::FindOrCreateArchetypeForAdd(uint32_t srcArchetypeId, uint32_t addSeq,
                                                const std::function<std::unique_ptr<IColumn>()> &makeColumn)
    {
        std::vector<uint32_t> dstSig = m_archetypes[srcArchetypeId]->m_signature;
        dstSig.insert(std::upper_bound(dstSig.begin(), dstSig.end(), addSeq), addSeq); // sorted insert

        if (const uint32_t found = FindArchetypeId(dstSig); found != INVALID_ARCHETYPE_ID)
            return found;

        auto archetype = std::make_unique<Archetype>();
        archetype->m_signature = dstSig;
        for (const auto &[seq, column] : m_archetypes[srcArchetypeId]->m_table.Columns())
            archetype->m_table.AddColumn(seq, column->CloneEmpty());
        archetype->m_table.AddColumn(addSeq, makeColumn());

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

    void World::Validate() const
    {
#ifndef NDEBUG
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
        ENGINE_ASSERT(totalRows == EntityCount(), "World::Validate: total archetype rows != alive entity count");

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
