#include <catch2/catch_test_macros.hpp>

#include <engine/core/ecs/CommandBuffer.hpp>
#include <engine/core/ecs/Query.hpp>
#include <engine/core/ecs/StorageKind.hpp>
#include <engine/core/ecs/World.hpp>

using namespace engine;

namespace
{
    struct Position
    {
        float m_x = 0.0f;
    };

    struct Velocity
    {
        float m_dx = 0.0f;
    };

    // sparse-routed, for cross-storage record-order coverage
    struct SparseTag
    {
        int m_value = 0;
    };

    struct HookState
    {
        int m_addCount = 0;
        int m_removeCount = 0;
    };

    HookState g_hooks;

    void ResetHooks() { g_hooks = HookState{}; }

    void OnAdd(World &, Entity, void *) noexcept { g_hooks.m_addCount++; }
    void OnRemove(World &, Entity, void *) noexcept { g_hooks.m_removeCount++; }
}

template <>
struct engine::ComponentStorageKind<SparseTag>
{
    static constexpr StorageKind VALUE = StorageKind::SparseSet;
};

TEST_CASE("CommandBuffer deferred spawn is alive but unplaced until flush, then placed", "[core][ecs][commands]")
{
    World world;
    Entity e = world.Commands().Spawn();

    REQUIRE(world.IsAlive(e));
    REQUIRE(world.GetComponent<Position>(e) == nullptr);
    REQUIRE_FALSE(world.HasComponent<Position>(e));

    std::size_t seen = 0;
    for ([[maybe_unused]] auto [queried] : Query<Entity>(world))
        ++seen;
    REQUIRE(seen == 0); // unplaced: invisible to Query<Entity>

    world.FlushCommands();

    REQUIRE(world.IsAlive(e));
    seen = 0;
    for ([[maybe_unused]] auto [queried] : Query<Entity>(world))
        ++seen;
    REQUIRE(seen == 1);
    world.Validate();
}

TEST_CASE("CommandBuffer deferred add lands with correct value and ticks stamped at flush", "[core][ecs][commands]")
{
    World world;
    Entity e = world.Commands().Spawn();
    world.Commands().AddComponent(e, Position{3.0f});

    REQUIRE(world.GetComponentMeta<Position>(e) == nullptr); // not yet placed/added

    world.AdvanceTick();
    const uint32_t flushTick = world.CurrentTick();
    world.FlushCommands();

    REQUIRE(world.GetComponent<Position>(e)->m_x == 3.0f);
    REQUIRE(world.GetComponentMeta<Position>(e)->m_changedTick == flushTick);
    REQUIRE(world.GetComponentMeta<Position>(e)->m_addedTick == flushTick);
    world.Validate();
}

TEST_CASE("CommandBuffer deferred add on an entity despawned before flush is a no-op, not a crash", "[core][ecs][commands]")
{
    World world;
    Entity e = world.Spawn();
    world.Commands().AddComponent(e, Position{1.0f});
    world.Despawn(e);

    world.FlushCommands(); // must not crash; e is dead by the time the add would apply
    world.Validate();
}

TEST_CASE("CommandBuffer deferred despawn of an already-dead entity is a no-op", "[core][ecs][commands]")
{
    World world;
    Entity e = world.Spawn();
    world.Despawn(e);

    world.Commands().Despawn(e);
    world.FlushCommands(); // must not crash
    world.Validate();
}

TEST_CASE("CommandBuffer spawn then despawn in one buffer applies in record order and leaves nothing behind", "[core][ecs][commands]")
{
    World world;
    Entity e = world.Commands().Spawn();
    world.Commands().Despawn(e);

    world.FlushCommands();

    REQUIRE_FALSE(world.IsAlive(e));
    world.Validate();
}

TEST_CASE("CommandBuffer hooks fire at flush, not at record", "[core][ecs][commands]")
{
    ResetHooks();
    World world;
    world.SetComponentHooks<Position>(OnAdd, OnRemove);

    Entity e = world.Commands().Spawn();
    world.Commands().AddComponent(e, Position{1.0f});

    REQUIRE(g_hooks.m_addCount == 0);

    world.FlushCommands();

    REQUIRE(g_hooks.m_addCount == 1);
    world.Validate();
}

TEST_CASE("CommandBuffer cross-type record order is preserved: add A, add B, remove A leaves only B", "[core][ecs][commands]")
{
    World world;
    Entity e = world.Spawn();
    world.Commands().AddComponent(e, Position{1.0f});
    world.Commands().AddComponent(e, Velocity{2.0f});
    world.Commands().RemoveComponent<Position>(e);

    world.FlushCommands();

    REQUIRE(world.GetComponent<Position>(e) == nullptr);
    REQUIRE(world.GetComponent<Velocity>(e)->m_dx == 2.0f);
    world.Validate();
}

TEST_CASE("CommandBuffer preserves record order across table and sparse component kinds", "[core][ecs][commands]")
{
    World world;
    Entity e = world.Spawn();
    world.Commands().AddComponent(e, Position{1.0f});
    world.Commands().AddComponent(e, SparseTag{7});

    world.FlushCommands();

    REQUIRE(world.GetComponent<Position>(e)->m_x == 1.0f);
    REQUIRE(world.GetComponent<SparseTag>(e)->m_value == 7);
    world.Validate();
}

TEST_CASE("CommandBuffer arena growth: payload pointers survive a block append", "[core][ecs][commands]")
{
    World world;
    Entity e = world.Spawn();

    for (int i = 0; i < 2000; ++i)
        world.Commands().AddComponent(e, Position{static_cast<float>(i)});

    REQUIRE(world.Commands().PendingCount() == 2000);
    world.FlushCommands();

    REQUIRE(world.GetComponent<Position>(e)->m_x == 1999.0f); // last recorded add wins
    world.Validate();
}

TEST_CASE("CommandBuffer FlushCommands on an empty buffer is a no-op", "[core][ecs][commands]")
{
    World world;
    REQUIRE(world.Commands().IsEmpty());
    world.FlushCommands();
    REQUIRE(world.Commands().IsEmpty());
    world.Validate();
}

TEST_CASE("CommandBuffer IsEmpty is true after flush, and a second flush changes nothing", "[core][ecs][commands]")
{
    World world;
    Entity e = world.Commands().Spawn();
    world.Commands().AddComponent(e, Position{5.0f});

    world.FlushCommands();
    REQUIRE(world.Commands().IsEmpty());

    world.FlushCommands(); // second flush: nothing to do
    REQUIRE(world.GetComponent<Position>(e)->m_x == 5.0f);
    world.Validate();
}

TEST_CASE("World unplaced entity: reads are null/false and Despawn before flush leaves no trace", "[core][ecs][commands]")
{
    World world;
    Entity e = world.Commands().Spawn();

    REQUIRE(world.IsAlive(e));
    REQUIRE(world.GetComponent<Position>(e) == nullptr);
    REQUIRE(world.GetComponentMeta<Position>(e) == nullptr);
    REQUIRE_FALSE(world.HasComponent<Position>(e));

    world.Despawn(e);
    REQUIRE_FALSE(world.IsAlive(e));
    world.Validate();

    world.FlushCommands(); // the buffered Spawn command targets a now-dead entity; must no-op
    world.Validate();
}
