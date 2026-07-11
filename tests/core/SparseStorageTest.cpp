#include <catch2/catch_test_macros.hpp>

#include <engine/core/ecs/Entity.hpp>
#include <engine/core/ecs/SparseStorage.hpp>

#include <random>
#include <unordered_set>
#include <vector>

using namespace engine;

namespace
{
    struct Velocity
    {
        float m_dx = 0.0f;
    };

    Entity MakeEntity(uint32_t index, uint32_t generation = 0)
    {
        return Entity{index, generation};
    }
}

TEST_CASE("SparseStorage Insert/Get/Contains", "[core][ecs][storage]")
{
    SparseStorage<Velocity> storage;
    Entity                  e = MakeEntity(3);

    REQUIRE_FALSE(storage.Contains(e));
    REQUIRE(storage.Get(e) == nullptr);

    storage.Insert(e, Velocity{5.0f});

    REQUIRE(storage.Contains(e));
    REQUIRE(storage.Get(e) != nullptr);
    REQUIRE(storage.Get(e)->m_dx == 5.0f);
    REQUIRE(storage.Size() == 1);
    storage.Validate();
}

TEST_CASE("SparseStorage Insert on an already-present entity overwrites", "[core][ecs][storage]")
{
    SparseStorage<Velocity> storage;
    Entity                  e = MakeEntity(1);

    storage.Insert(e, Velocity{1.0f});
    storage.Insert(e, Velocity{2.0f});

    REQUIRE(storage.Size() == 1); // did not grow
    REQUIRE(storage.Get(e)->m_dx == 2.0f);
    storage.Validate();
}

TEST_CASE("SparseStorage Remove keeps dense packed and sparse correct for the swapped survivor", "[core][ecs][storage]")
{
    SparseStorage<Velocity> storage;
    Entity                  e0 = MakeEntity(0);
    Entity                  e1 = MakeEntity(1);
    Entity                  e2 = MakeEntity(2);

    storage.Insert(e0, Velocity{0.0f});
    storage.Insert(e1, Velocity{1.0f});
    storage.Insert(e2, Velocity{2.0f});

    storage.Remove(e0); // swaps e2 (last dense) into e0's dense slot

    REQUIRE(storage.Size() == 2);
    REQUIRE_FALSE(storage.Contains(e0));
    REQUIRE(storage.Contains(e1));
    REQUIRE(storage.Contains(e2));
    REQUIRE(storage.Get(e2)->m_dx == 2.0f);
    storage.Validate();
}

TEST_CASE("SparseStorage Contains rejects a stale handle and a never-inserted entity", "[core][ecs][storage]")
{
    SparseStorage<Velocity> storage;
    Entity                  e = MakeEntity(4, 0);

    storage.Insert(e, Velocity{9.0f});

    Entity staleGeneration = MakeEntity(4, 1); // same index, older/different generation than stored
    REQUIRE_FALSE(storage.Contains(staleGeneration));
    REQUIRE(storage.Get(staleGeneration) == nullptr);

    Entity neverInserted = MakeEntity(99, 0);
    REQUIRE_FALSE(storage.Contains(neverInserted));
    REQUIRE(storage.Get(neverInserted) == nullptr);
}

TEST_CASE("SparseStorage Meta relocates with its value across Remove", "[core][ecs][storage]")
{
    SparseStorage<Velocity> storage;
    Entity                  e0 = MakeEntity(0);
    Entity                  e1 = MakeEntity(1);

    storage.Insert(e0, Velocity{0.0f}, ComponentMeta{10, 20});
    storage.Insert(e1, Velocity{1.0f}, ComponentMeta{30, 40});

    storage.Remove(e0); // e1 (last) swaps into e0's slot

    REQUIRE(storage.Meta(e1)->m_changedTick == 30);
    REQUIRE(storage.Meta(e1)->m_addedTick == 40);
}

TEST_CASE("SparseStorage Insert of a null entity is a safe no-op", "[core][ecs][storage]")
{
    // NULL_ENTITY.m_index == UINT32_MAX -> would overflow sparse growth if unguarded.
    // Release path must early-return without touching m_sparse (Debug also asserts).
    SparseStorage<Velocity> storage;
    storage.Insert(NULL_ENTITY, Velocity{1.0f});

    REQUIRE(storage.Size() == 0);
    REQUIRE_FALSE(storage.Contains(NULL_ENTITY));
    storage.Validate();
}

TEST_CASE("SparseStorage property: N random Insert/Remove preserve invariants", "[core][ecs][storage]")
{
    SparseStorage<Velocity>      storage;
    std::unordered_set<uint32_t> trackedIndices; // entities currently inserted (generation 0 pool)

    std::mt19937                    rng(987654321u);
    std::uniform_int_distribution<> action(0, 1);
    std::uniform_int_distribution<> indexPick(0, 63); // small pool -> frequent overlap/removal

    for (int i = 0; i < 5000; ++i)
    {
        const uint32_t index = static_cast<uint32_t>(indexPick(rng));
        Entity         e     = MakeEntity(index);

        if (action(rng) == 0)
        {
            storage.Insert(e, Velocity{static_cast<float>(index)});
            trackedIndices.insert(index);
        }
        else
        {
            storage.Remove(e);
            trackedIndices.erase(index);
        }

        REQUIRE(storage.Size() == trackedIndices.size());
        for (uint32_t idx : trackedIndices)
        {
            REQUIRE(storage.Contains(MakeEntity(idx)));
        }
        storage.Validate();
    }
}
