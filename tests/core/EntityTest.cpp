#include <catch2/catch_test_macros.hpp>

#include <engine/core/ecs/Entity.hpp>
#include <engine/core/ecs/EntityAllocator.hpp>

#include <cstdint>
#include <random>
#include <unordered_set>
#include <vector>

using namespace engine;

// friend accessor: pokes a slot's generation directly to reach the retirement path
// (near-UINT32_MAX generation) without 4 billion real Allocate/Free cycles.
namespace engine::detail
{
    struct EntityAllocatorTestAccess
    {
        static void SetGeneration(EntityAllocator& alloc, uint32_t index, uint32_t generation)
        {
            alloc.m_generations[index] = generation;
        }
    };
}

using engine::detail::EntityAllocatorTestAccess;

TEST_CASE("null entity", "[core][ecs][entity]")
{
    REQUIRE(NULL_ENTITY.IsNull());
    REQUIRE(NULL_ENTITY.m_index == Entity::NULL_INDEX);

    EntityAllocator alloc;
    REQUIRE_FALSE(alloc.IsAlive(NULL_ENTITY));
    alloc.Free(NULL_ENTITY); // safe no-op
    REQUIRE(alloc.AliveCount() == 0);
}

TEST_CASE("Allocate returns unique live entities never using NULL_INDEX", "[core][ecs][entity]")
{
    EntityAllocator alloc;
    Entity          a = alloc.Allocate();
    Entity          b = alloc.Allocate();

    REQUIRE(a != b);
    REQUIRE_FALSE(a.IsNull());
    REQUIRE_FALSE(b.IsNull());
    REQUIRE(alloc.IsAlive(a));
    REQUIRE(alloc.IsAlive(b));
    REQUIRE(alloc.AliveCount() == 2);
}

TEST_CASE("Free makes an entity immediately not-alive", "[core][ecs][entity]")
{
    EntityAllocator alloc;
    Entity          a = alloc.Allocate();

    alloc.Free(a);
    REQUIRE_FALSE(alloc.IsAlive(a));
    REQUIRE(alloc.AliveCount() == 0);
}

TEST_CASE("Free on an already-stale entity is a safe no-op", "[core][ecs][entity]")
{
    EntityAllocator alloc;
    Entity          a = alloc.Allocate();

    alloc.Free(a);
    alloc.Free(a); // second free of the same stale handle -- must not double-decrement
    REQUIRE(alloc.AliveCount() == 0);
}

TEST_CASE("reuse increments generation and FIFO order is respected", "[core][ecs][entity]")
{
    EntityAllocator alloc;
    Entity          a = alloc.Allocate(); // index 0
    Entity          b = alloc.Allocate(); // index 1

    alloc.Free(a);
    alloc.Free(b);

    // FIFO -> oldest-freed (a's index) comes back first
    Entity reused1 = alloc.Allocate();
    REQUIRE(reused1.m_index == a.m_index);
    REQUIRE(reused1.m_generation == a.m_generation + 1);

    Entity reused2 = alloc.Allocate();
    REQUIRE(reused2.m_index == b.m_index);
    REQUIRE(reused2.m_generation == b.m_generation + 1);
}

TEST_CASE("stale entity (old generation) reports not-alive after reuse", "[core][ecs][entity]")
{
    EntityAllocator alloc;
    Entity          a = alloc.Allocate();

    alloc.Free(a);
    Entity reused = alloc.Allocate();

    REQUIRE_FALSE(alloc.IsAlive(a)); // old handle, superseded generation
    REQUIRE(alloc.IsAlive(reused));
    alloc.Free(a); // safe no-op, must not disturb the live reused entity
    REQUIRE(alloc.IsAlive(reused));
}

TEST_CASE("retired slot (generation exhausted) is never handed out again", "[core][ecs][entity]")
{
    EntityAllocator alloc;
    Entity          a = alloc.Allocate();

    EntityAllocatorTestAccess::SetGeneration(alloc, a.m_index, UINT32_MAX - 1);
    Entity nearOverflow{a.m_index, UINT32_MAX - 1};

    alloc.Free(nearOverflow); // increment would reach UINT32_MAX -> retire, do not re-enqueue
    REQUIRE_FALSE(alloc.IsAlive(nearOverflow));
    REQUIRE(alloc.AliveCount() == 0);

    // forged tombstone-generation handle: never alive, Free on it is a safe no-op
    Entity forged{a.m_index, UINT32_MAX};
    REQUIRE_FALSE(alloc.IsAlive(forged));
    alloc.Free(forged);
    REQUIRE(alloc.AliveCount() == 0);

    // slot must not come back from the free list -- next Allocate uses a fresh index
    Entity next = alloc.Allocate();
    REQUIRE(next.m_index != a.m_index);
}

TEST_CASE("property: N random Allocate/Free sequences preserve allocator invariants", "[core][ecs][entity]")
{
    EntityAllocator          alloc;
    std::unordered_set<uint64_t> liveKeys; // (index,generation) packed
    std::vector<Entity>      liveHandles;
    std::vector<Entity>      staleHandles; // freed handles -- must stay dead forever

    auto pack = [](Entity e) -> uint64_t
    {
        return (static_cast<uint64_t>(e.m_index) << 32) | e.m_generation;
    };

    std::mt19937                    rng(1234567u);
    std::uniform_int_distribution<> action(0, 1);

    for (int i = 0; i < 5000; ++i)
    {
        if (liveHandles.empty() || action(rng) == 0)
        {
            Entity e = alloc.Allocate();

            REQUIRE_FALSE(e.IsNull());
            REQUIRE(liveKeys.find(pack(e)) == liveKeys.end()); // never equal to a currently-live entity

            liveKeys.insert(pack(e));
            liveHandles.push_back(e);
        }
        else
        {
            std::uniform_int_distribution<std::size_t> pick(0, liveHandles.size() - 1);
            std::size_t                                idx = pick(rng);
            Entity                                     e   = liveHandles[idx];

            alloc.Free(e);
            liveKeys.erase(pack(e));
            liveHandles[idx] = liveHandles.back();
            liveHandles.pop_back();
            staleHandles.push_back(e);
        }

        // invariants after every op -- both directions of IsAlive <=> tracked-live set
        // (stale sweep bounded to the most recent frees per op; full sweep once at the end,
        //  else the test goes quadratic in Catch2 REQUIRE overhead)
        REQUIRE(alloc.AliveCount() == liveHandles.size());
        for (Entity e : liveHandles)
        {
            REQUIRE(alloc.IsAlive(e));
        }
        const std::size_t staleWindow = staleHandles.size() < 32 ? staleHandles.size() : 32;
        for (std::size_t s = staleHandles.size() - staleWindow; s < staleHandles.size(); ++s)
        {
            REQUIRE_FALSE(alloc.IsAlive(staleHandles[s]));
        }
    }

    // full backward sweep: every handle ever freed must still be dead
    for (Entity e : staleHandles)
    {
        REQUIRE_FALSE(alloc.IsAlive(e));
    }
}
