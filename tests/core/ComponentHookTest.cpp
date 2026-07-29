#include <catch2/catch_test_macros.hpp>

#include <engine/core/ecs/Column.hpp>
#include <engine/core/ecs/ComponentMeta.hpp>
#include <engine/core/ecs/ComponentRegistry.hpp>
#include <engine/core/ecs/Entity.hpp>
#include <engine/core/ecs/SparseStorage.hpp>
#include <engine/core/ecs/StorageKind.hpp>
#include <engine/core/ecs/TypeId.hpp>
#include <engine/core/ecs/World.hpp>

#include <cstdint>
#include <vector>

using namespace engine;

namespace
{
    struct Position
    {
        float m_x = 0.0f;
    };

    Entity MakeEntity(uint32_t index, uint32_t generation = 0)
    {
        return Entity{index, generation};
    }

    // Table-kind, hooked
    struct HookedTable
    {
        int m_value = 0;
    };

    // sparse-routed, hooked
    struct HookedSparse
    {
        int m_value = 0;
    };

    // Table-kind, only on_remove ever registered
    struct PartiallyHooked
    {
        int m_value = 0;
    };

    // Table-kind, never given hooks; the second component in the migration test
    struct PlainSecond
    {
        int m_value = 0;
    };

    // sparse-routed, hooked; second sparse type for the despawn-sweep ordering test
    struct HookedSparse2
    {
        int m_value = 0;
    };

    struct HookState
    {
        int m_addCount = 0;
        int m_removeCount = 0;
        int m_addSeq = -1;
        int m_removeSeq = -1;
        int m_lastAddValue = 0;
        int m_lastRemoveValue = 0;
    };

    int g_sequence = 0;
    HookState g_tableHooks;
    HookState g_sparseHooks;
    HookState g_partialHooks;
    HookState g_sparse2Hooks;
    std::vector<uint32_t> g_removeOrder;

    // hooks reach state through contexts per the TDD hook contract; contexts don't exist until
    // unit 09, so these tests use namespace-scope counters instead (out of scope, see plan 08)
    void ResetHookState()
    {
        g_sequence = 0;
        g_tableHooks = HookState{};
        g_sparseHooks = HookState{};
        g_partialHooks = HookState{};
        g_sparse2Hooks = HookState{};
        g_removeOrder.clear();
    }

    void OnHookedTableAdd(World &, Entity, void *data) noexcept
    {
        g_tableHooks.m_addCount++;
        g_tableHooks.m_addSeq = g_sequence++;
        g_tableHooks.m_lastAddValue = static_cast<HookedTable *>(data)->m_value;
    }

    void OnHookedTableRemove(World &, Entity, void *data) noexcept
    {
        g_tableHooks.m_removeCount++;
        g_tableHooks.m_removeSeq = g_sequence++;
        g_tableHooks.m_lastRemoveValue = static_cast<HookedTable *>(data)->m_value;
    }

    void OnHookedSparseAdd(World &, Entity, void *data) noexcept
    {
        g_sparseHooks.m_addCount++;
        g_sparseHooks.m_addSeq = g_sequence++;
        g_sparseHooks.m_lastAddValue = static_cast<HookedSparse *>(data)->m_value;
    }

    void OnHookedSparseRemove(World &, Entity, void *data) noexcept
    {
        g_sparseHooks.m_removeCount++;
        g_sparseHooks.m_removeSeq = g_sequence++;
        g_sparseHooks.m_lastRemoveValue = static_cast<HookedSparse *>(data)->m_value;
        g_removeOrder.push_back(TypeIdOf<HookedSparse>().m_seq);
    }

    void OnPartialRemove(World &, Entity, void *data) noexcept
    {
        g_partialHooks.m_removeCount++;
        g_partialHooks.m_lastRemoveValue = static_cast<PartiallyHooked *>(data)->m_value;
    }

    void OnHookedSparse2Remove(World &, Entity, void *data) noexcept
    {
        g_sparse2Hooks.m_removeCount++;
        g_sparse2Hooks.m_lastRemoveValue = static_cast<HookedSparse2 *>(data)->m_value;
        g_removeOrder.push_back(TypeIdOf<HookedSparse2>().m_seq);
    }
}

template <>
struct engine::ComponentStorageKind<HookedSparse>
{
    static constexpr StorageKind VALUE = StorageKind::SparseSet;
};

template <>
struct engine::ComponentStorageKind<HookedSparse2>
{
    static constexpr StorageKind VALUE = StorageKind::SparseSet;
};

TEST_CASE("Column DataAt returns the same address as Get for a given row", "[core][ecs][hooks]")
{
    Column<Position> column;
    column.Push(Position{1.0f}, ComponentMeta{});
    column.Push(Position{2.0f}, ComponentMeta{});

    REQUIRE(column.DataAt(0) == static_cast<void *>(&column.Get(0)));
    REQUIRE(column.DataAt(1) == static_cast<void *>(&column.Get(1)));
    REQUIRE(static_cast<Position *>(column.DataAt(1))->m_x == 2.0f);
}

TEST_CASE("Column DataAt stays correct after SwapRemove reshuffles rows", "[core][ecs][hooks]")
{
    Column<Position> column;
    column.Push(Position{1.0f});
    column.Push(Position{2.0f});
    column.Push(Position{3.0f});

    column.SwapRemove(0); // row 2 (3.0f) moves into hole 0

    REQUIRE(static_cast<Position *>(column.DataAt(0))->m_x == 3.0f);
    REQUIRE(column.DataAt(0) == static_cast<void *>(&column.Get(0)));
    REQUIRE(column.Size() == 2);
}

TEST_CASE("SparseStorage DataFor matches Get and is null for an absent entity", "[core][ecs][hooks]")
{
    SparseStorage<Position> storage;
    Entity a = MakeEntity(0);
    Entity b = MakeEntity(1);

    REQUIRE(storage.DataFor(a) == nullptr);

    storage.Insert(a, Position{5.0f});
    REQUIRE(storage.DataFor(a) == static_cast<void *>(storage.Get(a)));
    REQUIRE(static_cast<Position *>(storage.DataFor(a))->m_x == 5.0f);
    REQUIRE(storage.DataFor(b) == nullptr);
    storage.Validate();
}

TEST_CASE("SparseStorage DataFor stays correct after Remove reshuffles the dense array", "[core][ecs][hooks]")
{
    SparseStorage<Position> storage;
    Entity a = MakeEntity(0);
    Entity b = MakeEntity(1);
    Entity c = MakeEntity(2);

    storage.Insert(a, Position{1.0f});
    storage.Insert(b, Position{2.0f});
    storage.Insert(c, Position{3.0f});

    storage.Remove(a); // c (last) swaps into a's slot

    REQUIRE(storage.DataFor(a) == nullptr);
    REQUIRE(static_cast<Position *>(storage.DataFor(c))->m_x == 3.0f);
    REQUIRE(storage.DataFor(c) == static_cast<void *>(storage.Get(c)));
    storage.Validate();
}

TEST_CASE("ComponentRegistry SetHooks stores both hooks without disturbing other fields", "[core][ecs][hooks]")
{
    ComponentRegistry registry;
    registry.SetHooks<HookedTable>(OnHookedTableAdd, OnHookedTableRemove);

    const ComponentInfo *info = registry.Find<HookedTable>();
    REQUIRE(info != nullptr);
    REQUIRE(info->m_onAdd == OnHookedTableAdd);
    REQUIRE(info->m_onRemove == OnHookedTableRemove);
    REQUIRE(info->m_storage == StorageKind::Table); // untouched by SetHooks
    REQUIRE(info->m_makeColumn != nullptr);          // untouched by SetHooks
    registry.Validate();
}

TEST_CASE("ComponentRegistry SetHooks attaches to a record Register already created, without a second entry", "[core][ecs][hooks]")
{
    // code review 2026-07-27, finding 4: SetHooks used to skip its own frozen check whenever T
    // already had a record (e.g. from an earlier Register<T>() via AddComponent), because that
    // path never reached the assert at all. SetHooks now always calls Register<T>() itself,
    // so the same find-or-insert logic runs regardless of who touched the record first.
    ComponentRegistry registry;
    registry.Register<HookedTable>(); // record exists, no hooks yet
    REQUIRE(registry.Size() == 1);

    registry.SetHooks<HookedTable>(OnHookedTableAdd, OnHookedTableRemove);
    REQUIRE(registry.Size() == 1); // still one record, not two
    const ComponentInfo *info = registry.Find<HookedTable>();
    REQUIRE(info->m_onAdd == OnHookedTableAdd);
    REQUIRE(info->m_onRemove == OnHookedTableRemove);
    registry.Validate();
}

TEST_CASE("ComponentRegistry SetHooks is idempotent on the record it creates: calling it twice keeps one entry", "[core][ecs][hooks]")
{
    ComponentRegistry registry;
    registry.SetHooks<HookedTable>(OnHookedTableAdd, OnHookedTableRemove);
    REQUIRE(registry.Size() == 1);

    registry.SetHooks<HookedTable>(OnHookedTableAdd, OnHookedTableRemove);
    REQUIRE(registry.Size() == 1);
    registry.Validate();
}

TEST_CASE("World AddComponent fires on_add exactly once for a new table-kind component", "[core][ecs][hooks]")
{
    ResetHookState();
    World world;
    world.SetComponentHooks<HookedTable>(OnHookedTableAdd, OnHookedTableRemove);

    Entity e = world.Spawn();
    world.AddComponent(e, HookedTable{5});

    REQUIRE(g_tableHooks.m_addCount == 1);
    REQUIRE(g_tableHooks.m_removeCount == 0);
    REQUIRE(g_tableHooks.m_lastAddValue == 5);
    world.Validate();
}

TEST_CASE("World RemoveComponent fires on_remove exactly once for a table-kind component", "[core][ecs][hooks]")
{
    ResetHookState();
    World world;
    world.SetComponentHooks<HookedTable>(OnHookedTableAdd, OnHookedTableRemove);

    Entity e = world.Spawn();
    world.AddComponent(e, HookedTable{9});
    world.RemoveComponent<HookedTable>(e);

    REQUIRE(g_tableHooks.m_addCount == 1);
    REQUIRE(g_tableHooks.m_removeCount == 1);
    REQUIRE(g_tableHooks.m_lastRemoveValue == 9);
    world.Validate();
}

TEST_CASE("World AddComponent fires on_add exactly once for a new sparse-kind component", "[core][ecs][hooks]")
{
    ResetHookState();
    World world;
    world.SetComponentHooks<HookedSparse>(OnHookedSparseAdd, OnHookedSparseRemove);

    Entity e = world.Spawn();
    world.AddComponent(e, HookedSparse{3});

    REQUIRE(g_sparseHooks.m_addCount == 1);
    REQUIRE(g_sparseHooks.m_removeCount == 0);
    REQUIRE(g_sparseHooks.m_lastAddValue == 3);
    world.Validate();
}

TEST_CASE("World RemoveComponent fires on_remove exactly once for a sparse-kind component", "[core][ecs][hooks]")
{
    ResetHookState();
    World world;
    world.SetComponentHooks<HookedSparse>(OnHookedSparseAdd, OnHookedSparseRemove);

    Entity e = world.Spawn();
    world.AddComponent(e, HookedSparse{4});
    world.RemoveComponent<HookedSparse>(e);

    REQUIRE(g_sparseHooks.m_addCount == 1);
    REQUIRE(g_sparseHooks.m_removeCount == 1);
    REQUIRE(g_sparseHooks.m_lastRemoveValue == 4);
    world.Validate();
}

TEST_CASE("World AddComponent overwrite fires on_remove(old) then on_add(new), for both storage kinds", "[core][ecs][hooks]")
{
    ResetHookState();
    World world;
    world.SetComponentHooks<HookedTable>(OnHookedTableAdd, OnHookedTableRemove);
    world.SetComponentHooks<HookedSparse>(OnHookedSparseAdd, OnHookedSparseRemove);

    Entity e = world.Spawn();
    world.AddComponent(e, HookedTable{1});
    world.AddComponent(e, HookedSparse{10});
    ResetHookState(); // clear the initial-add firings; only the overwrite matters below

    world.AddComponent(e, HookedTable{2}); // overwrite
    REQUIRE(g_tableHooks.m_addCount == 1);
    REQUIRE(g_tableHooks.m_removeCount == 1);
    REQUIRE(g_tableHooks.m_removeSeq < g_tableHooks.m_addSeq); // remove(old) strictly before add(new)
    REQUIRE(g_tableHooks.m_lastRemoveValue == 1);              // outgoing value
    REQUIRE(g_tableHooks.m_lastAddValue == 2);                 // incoming value

    world.AddComponent(e, HookedSparse{20}); // overwrite
    REQUIRE(g_sparseHooks.m_addCount == 1);
    REQUIRE(g_sparseHooks.m_removeCount == 1);
    REQUIRE(g_sparseHooks.m_removeSeq < g_sparseHooks.m_addSeq);
    REQUIRE(g_sparseHooks.m_lastRemoveValue == 10);
    REQUIRE(g_sparseHooks.m_lastAddValue == 20);

    world.Validate();
}

TEST_CASE("World AddComponent overwrite leaves m_addedTick unchanged on a hooked component", "[core][ecs][hooks]")
{
    World world;
    world.SetComponentHooks<HookedTable>(OnHookedTableAdd, OnHookedTableRemove);

    Entity e = world.Spawn();
    world.AddComponent(e, HookedTable{1}); // added==changed==tick 1
    const uint32_t addedTick = world.GetComponentMeta<HookedTable>(e)->m_addedTick;

    world.AdvanceTick();
    world.AddComponent(e, HookedTable{2}); // overwrite: hooks fire, tick is NOT re-added

    REQUIRE(world.GetComponentMeta<HookedTable>(e)->m_addedTick == addedTick);
    REQUIRE(world.GetComponentMeta<HookedTable>(e)->m_changedTick == world.CurrentTick());
    world.Validate();
}

TEST_CASE("World RemoveComponent on an entity that never had the component fires nothing", "[core][ecs][hooks]")
{
    ResetHookState();
    World world;
    world.SetComponentHooks<HookedTable>(OnHookedTableAdd, OnHookedTableRemove);
    world.SetComponentHooks<HookedSparse>(OnHookedSparseAdd, OnHookedSparseRemove);

    Entity e = world.Spawn();
    world.RemoveComponent<HookedTable>(e);
    world.RemoveComponent<HookedSparse>(e);

    REQUIRE(g_tableHooks.m_removeCount == 0);
    REQUIRE(g_sparseHooks.m_removeCount == 0);
    world.Validate();
}

TEST_CASE("World AddComponent on a dead entity fires nothing", "[core][ecs][hooks]")
{
    ResetHookState();
    World world;
    world.SetComponentHooks<HookedTable>(OnHookedTableAdd, OnHookedTableRemove);

    Entity e = world.Spawn();
    world.Despawn(e);
    world.AddComponent(e, HookedTable{1});

    REQUIRE(g_tableHooks.m_addCount == 0);
    world.Validate();
}

TEST_CASE("World SetComponentHooks with a null on_add is a no-op; the other side still fires", "[core][ecs][hooks]")
{
    ResetHookState();
    World world;
    world.SetComponentHooks<PartiallyHooked>(nullptr, OnPartialRemove);

    Entity e = world.Spawn();
    world.AddComponent(e, PartiallyHooked{6}); // on_add is null -> no counter to move
    REQUIRE(g_partialHooks.m_removeCount == 0);

    world.RemoveComponent<PartiallyHooked>(e);
    REQUIRE(g_partialHooks.m_removeCount == 1);
    REQUIRE(g_partialHooks.m_lastRemoveValue == 6);
    world.Validate();
}

// the single most likely defect in this unit: MoveRowTo relocates a component that is neither
// arriving nor leaving, so it must never see FireOnAdd/FireOnRemove
TEST_CASE("World AddComponent: an archetype migration caused by a second component fires nothing for the first", "[core][ecs][hooks]")
{
    ResetHookState();
    World world;
    world.SetComponentHooks<HookedTable>(OnHookedTableAdd, OnHookedTableRemove);

    Entity e = world.Spawn();
    world.AddComponent(e, HookedTable{42});
    ResetHookState(); // clear the initial-add firing; only the migration matters below

    world.AddComponent(e, PlainSecond{1}); // forces {HookedTable} -> {HookedTable, PlainSecond}

    REQUIRE(g_tableHooks.m_addCount == 0);
    REQUIRE(g_tableHooks.m_removeCount == 0);
    REQUIRE(world.GetComponent<HookedTable>(e)->m_value == 42); // data itself survived the move
    world.Validate();
}

TEST_CASE("World SetComponentHooks<T> before Freeze keeps firing after Freeze", "[core][ecs][hooks]")
{
    ResetHookState();
    World world;
    world.SetComponentHooks<HookedTable>(OnHookedTableAdd, OnHookedTableRemove);
    world.FreezeComponents();

    Entity e = world.Spawn();
    world.AddComponent(e, HookedTable{7});
    REQUIRE(g_tableHooks.m_addCount == 1);
    REQUIRE(g_tableHooks.m_lastAddValue == 7);

    world.RemoveComponent<HookedTable>(e);
    REQUIRE(g_tableHooks.m_removeCount == 1);
    REQUIRE(g_tableHooks.m_lastRemoveValue == 7);
    world.Validate();
}

TEST_CASE("World Despawn fires on_remove once for a hooked table component and once for a hooked sparse component", "[core][ecs][hooks]")
{
    ResetHookState();
    World world;
    world.SetComponentHooks<HookedTable>(OnHookedTableAdd, OnHookedTableRemove);
    world.SetComponentHooks<HookedSparse>(OnHookedSparseAdd, OnHookedSparseRemove);

    Entity e = world.Spawn();
    world.AddComponent(e, HookedTable{11});
    world.AddComponent(e, HookedSparse{22});
    ResetHookState(); // clear the two initial-add firings; only despawn matters below

    world.Despawn(e);

    REQUIRE(g_tableHooks.m_removeCount == 1);
    REQUIRE(g_tableHooks.m_lastRemoveValue == 11);
    REQUIRE(g_sparseHooks.m_removeCount == 1);
    REQUIRE(g_sparseHooks.m_lastRemoveValue == 22);
    world.Validate();
}

TEST_CASE("World Despawn of an entity with no hooked components fires nothing", "[core][ecs][hooks]")
{
    ResetHookState();
    World world;
    world.SetComponentHooks<HookedTable>(OnHookedTableAdd, OnHookedTableRemove);
    world.SetComponentHooks<HookedSparse>(OnHookedSparseAdd, OnHookedSparseRemove);

    Entity e = world.Spawn(); // no components at all
    world.Despawn(e);

    REQUIRE(g_tableHooks.m_removeCount == 0);
    REQUIRE(g_sparseHooks.m_removeCount == 0);
    world.Validate();
}

TEST_CASE("World Despawn fires on_remove for an unhooked-but-present component with no crash", "[core][ecs][hooks]")
{
    World world;
    Entity e = world.Spawn(PlainSecond{5}); // PlainSecond never registers hooks
    world.Despawn(e);                       // must not crash; m_onRemove is null for it
    REQUIRE_FALSE(world.IsAlive(e));
    world.Validate();
}

TEST_CASE("World destructor fires on_remove exactly once per live entity for a hooked component", "[core][ecs][hooks]")
{
    ResetHookState();
    {
        World world;
        world.SetComponentHooks<HookedTable>(OnHookedTableAdd, OnHookedTableRemove);
        world.SetComponentHooks<HookedSparse>(OnHookedSparseAdd, OnHookedSparseRemove);

        for (int i = 0; i < 5; ++i)
        {
            Entity e = world.Spawn();
            world.AddComponent(e, HookedTable{i});
            world.AddComponent(e, HookedSparse{i * 10});
        }
        ResetHookState(); // clear the ten initial-add firings; only teardown matters below
    } // ~World fires here

    REQUIRE(g_tableHooks.m_removeCount == 5);
    REQUIRE(g_sparseHooks.m_removeCount == 5);
}

TEST_CASE("World Despawn fires on_remove for two hooked sparse components in ascending-seq order, repeatably", "[core][ecs][hooks]")
{
    // unit 10, chunk 6: m_sparseStorages is unordered_map bucket order, so the sweep now sorts
    // seqs before firing; run it a few times to catch any bucket-order dependence
    const uint32_t seqA = TypeIdOf<HookedSparse>().m_seq;
    const uint32_t seqB = TypeIdOf<HookedSparse2>().m_seq;
    const std::vector<uint32_t> expected = seqA < seqB ? std::vector<uint32_t>{seqA, seqB}
                                                        : std::vector<uint32_t>{seqB, seqA};

    for (int run = 0; run < 3; ++run)
    {
        ResetHookState();
        World world;
        world.SetComponentHooks<HookedSparse>(OnHookedSparseAdd, OnHookedSparseRemove);
        world.SetComponentHooks<HookedSparse2>(nullptr, OnHookedSparse2Remove);

        Entity e = world.Spawn();
        world.AddComponent(e, HookedSparse{1});
        world.AddComponent(e, HookedSparse2{2});
        ResetHookState();

        world.Despawn(e);

        REQUIRE(g_sparseHooks.m_removeCount == 1);
        REQUIRE(g_sparse2Hooks.m_removeCount == 1);
        REQUIRE(g_removeOrder == expected);
        world.Validate();
    }
}

TEST_CASE("World destructor of an empty world fires nothing", "[core][ecs][hooks]")
{
    ResetHookState();
    {
        World world;
        world.SetComponentHooks<HookedTable>(OnHookedTableAdd, OnHookedTableRemove);
    }

    REQUIRE(g_tableHooks.m_removeCount == 0);
}

TEST_CASE("World destructor fires hooks for a component removed by an earlier explicit Despawn only once", "[core][ecs][hooks]")
{
    ResetHookState();
    {
        World world;
        world.SetComponentHooks<HookedTable>(OnHookedTableAdd, OnHookedTableRemove);

        Entity a = world.Spawn();
        world.AddComponent(a, HookedTable{1});
        Entity b = world.Spawn();
        world.AddComponent(b, HookedTable{2});

        ResetHookState();
        world.Despawn(a); // fires on_remove for a's HookedTable now
        REQUIRE(g_tableHooks.m_removeCount == 1);
        world.Validate();
    } // ~World fires here for b only; a is already gone

    REQUIRE(g_tableHooks.m_removeCount == 2); // 1 from Despawn(a) + 1 from teardown of b
}
