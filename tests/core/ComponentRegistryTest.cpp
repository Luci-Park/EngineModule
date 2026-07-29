#include <catch2/catch_test_macros.hpp>

#include <engine/core/ecs/Column.hpp>
#include <engine/core/ecs/ComponentInfo.hpp>
#include <engine/core/ecs/ComponentRegistry.hpp>
#include <engine/core/ecs/Entity.hpp>
#include <engine/core/ecs/SparseStorage.hpp>
#include <engine/core/ecs/StorageKind.hpp>
#include <engine/core/ecs/TypeId.hpp>
#include <engine/core/ecs/World.hpp>

#include <algorithm>
#include <array>
#include <random>

using namespace engine;

namespace
{
    struct Position
    {
        float m_x = 0.0f;
    };

    // sparse-routed component
    struct Frozen
    {
        int m_value = 0;
    };

    struct CompA
    {
        int m_a = 0;
    };
    struct CompB
    {
        int m_b = 0;
    };
    struct CompC
    {
        int m_c = 0;
    };
    struct CompD
    {
        int m_d = 0;
    };
}

template <>
struct engine::ComponentStorageKind<Frozen>
{
    static constexpr StorageKind VALUE = StorageKind::SparseSet;
};

TEST_CASE("MakeComponentInfo fills identity, size and storage for a Table-kind component", "[core][ecs][component-info]")
{
    const ComponentInfo info = MakeComponentInfo<Position>();
    const TypeId id = TypeIdOf<Position>();

    REQUIRE(info.m_seq == id.m_seq);
    REQUIRE(info.m_hash == id.m_hash);
    REQUIRE(info.m_name == id.m_name);
    REQUIRE(info.m_size == sizeof(Position));
    REQUIRE(info.m_align == alignof(Position));
    REQUIRE(info.m_storage == StorageKind::Table);
    REQUIRE(info.m_onAdd == nullptr);
    REQUIRE(info.m_onRemove == nullptr);
}

TEST_CASE("MakeComponentInfo fills identity, size and storage for a SparseSet-kind component", "[core][ecs][component-info]")
{
    const ComponentInfo info = MakeComponentInfo<Frozen>();
    const TypeId id = TypeIdOf<Frozen>();

    REQUIRE(info.m_seq == id.m_seq);
    REQUIRE(info.m_hash == id.m_hash);
    REQUIRE(info.m_name == id.m_name);
    REQUIRE(info.m_size == sizeof(Frozen));
    REQUIRE(info.m_align == alignof(Frozen));
    REQUIRE(info.m_storage == StorageKind::SparseSet);
}

TEST_CASE("ComponentInfo::m_makeColumn builds an empty column keyed by the same seq", "[core][ecs][component-info]")
{
    const ComponentInfo info = MakeComponentInfo<Position>();
    std::unique_ptr<IColumn> column = info.m_makeColumn();

    REQUIRE(column != nullptr);
    REQUIRE(column->ComponentSeq() == info.m_seq);
    REQUIRE(column->Size() == 0);
}

TEST_CASE("ComponentInfo::m_makeSparseStorage builds an empty storage", "[core][ecs][component-info]")
{
    const ComponentInfo info = MakeComponentInfo<Frozen>();
    std::unique_ptr<ISparseStorage> storage = info.m_makeSparseStorage();

    REQUIRE(storage != nullptr);
    REQUIRE(storage->Size() == 0);
    REQUIRE_FALSE(storage->Contains(Entity{0, 0}));
    storage->Validate();
}

TEST_CASE("ComponentRegistry Register is idempotent and returns the same record", "[core][ecs][component-registry]")
{
    ComponentRegistry registry;
    const ComponentInfo *first = registry.Register<Position>();
    REQUIRE(registry.Size() == 1);

    const ComponentInfo *second = registry.Register<Position>();
    REQUIRE(first == second);
    REQUIRE(registry.Size() == 1);
    registry.Validate();
}

TEST_CASE("ComponentRegistry Find is null for an unknown seq, non-null after Register", "[core][ecs][component-registry]")
{
    ComponentRegistry registry;
    REQUIRE(registry.Find(TypeIdOf<Position>().m_seq) == nullptr);
    REQUIRE_FALSE(registry.Contains(TypeIdOf<Position>().m_seq));

    registry.Register<Position>();
    const ComponentInfo *found = registry.Find(TypeIdOf<Position>().m_seq);
    REQUIRE(found != nullptr);
    REQUIRE(found->m_seq == TypeIdOf<Position>().m_seq);
    REQUIRE(registry.Contains(TypeIdOf<Position>().m_seq));
    registry.Validate();
}

TEST_CASE("ComponentRegistry Find<T> matches Find(seq)", "[core][ecs][component-registry]")
{
    ComponentRegistry registry;
    registry.Register<Frozen>();

    const ComponentInfo *bySeq = registry.Find(TypeIdOf<Frozen>().m_seq);
    const ComponentInfo *byType = registry.Find<Frozen>();
    REQUIRE(bySeq == byType);
    registry.Validate();
}

// Get()/Register() assert on an unknown-after-freeze type; ENGINE_ASSERT has no catchable
// failure path in this codebase (see AllocatorTest.cpp's non-fatal-query convention), so that
// branch is documented, not exercised. Find()/Contains()/IsFrozen() are the non-fatal
// counterparts used to verify the surrounding behavior instead.
TEST_CASE("ComponentRegistry IsFrozen reflects Freeze; re-registering a known type after freeze still succeeds", "[core][ecs][component-registry]")
{
    ComponentRegistry registry;
    REQUIRE_FALSE(registry.IsFrozen());

    const ComponentInfo *before = registry.Register<Position>();
    registry.Freeze();
    REQUIRE(registry.IsFrozen());

    const ComponentInfo *after = registry.Register<Position>();
    REQUIRE(before == after);
    REQUIRE(registry.Size() == 1);
    registry.Validate();
}

TEST_CASE("ComponentRegistry property: registering N distinct types in random order keeps Size and Contains consistent", "[core][ecs][component-registry]")
{
    std::mt19937 rng(20260726u);
    std::array<uint32_t, 4> order = {0, 1, 2, 3};

    for (int trial = 0; trial < 200; ++trial)
    {
        ComponentRegistry registry;
        std::shuffle(order.begin(), order.end(), rng);

        std::size_t registered = 0;
        for (uint32_t idx : order)
        {
            switch (idx)
            {
            case 0:
                registry.Register<CompA>();
                break;
            case 1:
                registry.Register<CompB>();
                break;
            case 2:
                registry.Register<CompC>();
                break;
            default:
                registry.Register<CompD>();
                break;
            }
            ++registered;
            REQUIRE(registry.Size() == registered);
            registry.Validate();
        }

        REQUIRE(registry.Contains(TypeIdOf<CompA>().m_seq));
        REQUIRE(registry.Contains(TypeIdOf<CompB>().m_seq));
        REQUIRE(registry.Contains(TypeIdOf<CompC>().m_seq));
        REQUIRE(registry.Contains(TypeIdOf<CompD>().m_seq));

        std::shuffle(order.begin(), order.end(), rng);
        for (uint32_t idx : order)
        {
            switch (idx)
            {
            case 0:
                registry.Register<CompA>();
                break;
            case 1:
                registry.Register<CompB>();
                break;
            case 2:
                registry.Register<CompC>();
                break;
            default:
                registry.Register<CompD>();
                break;
            }
            REQUIRE(registry.Size() == 4);
        }
        registry.Validate();
    }
}

TEST_CASE("World AddComponent registers the component with the right storage kind (table)", "[core][ecs][component-registry]")
{
    World world;
    Entity e = world.Spawn();
    world.AddComponent(e, Position{1.0f});

    const ComponentInfo *info = world.FindComponentInfo(TypeIdOf<Position>().m_seq);
    REQUIRE(info != nullptr);
    REQUIRE(info->m_seq == TypeIdOf<Position>().m_seq);
    REQUIRE(info->m_storage == StorageKind::Table);
    world.Validate();
}

TEST_CASE("World AddComponent registers the component with the right storage kind (sparse)", "[core][ecs][component-registry]")
{
    World world;
    Entity e = world.Spawn();
    world.AddComponent(e, Frozen{1});

    const ComponentInfo *info = world.FindComponentInfo(TypeIdOf<Frozen>().m_seq);
    REQUIRE(info != nullptr);
    REQUIRE(info->m_storage == StorageKind::SparseSet);
    world.Validate();
}

// removal drops an existing column and needs no record, so it must not register. Registering
// here would make a defensive remove of a never-added type trip the frozen assert (below)
// while changing nothing.
TEST_CASE("World RemoveComponent does not register a type the entity never had", "[core][ecs][component-registry]")
{
    World world;
    Entity e = world.Spawn();

    REQUIRE(world.FindComponentInfo(TypeIdOf<CompA>().m_seq) == nullptr);
    world.RemoveComponent<CompA>(e);
    REQUIRE(world.FindComponentInfo(TypeIdOf<CompA>().m_seq) == nullptr);
    REQUIRE_FALSE(world.HasComponent<CompA>(e));
    world.Validate();
}

TEST_CASE("World RemoveComponent of a never-added type is safe after the registry is frozen", "[core][ecs][component-registry]")
{
    World world;
    Entity e = world.Spawn();
    world.AddComponent(e, Position{1.0f});
    world.FreezeComponents();

    world.RemoveComponent<CompB>(e); // never registered; must not assert, must not register
    REQUIRE(world.FindComponentInfo(TypeIdOf<CompB>().m_seq) == nullptr);
    REQUIRE(world.GetComponent<Position>(e)->m_x == 1.0f);
    world.Validate();
}

TEST_CASE("World FreezeComponents: a type registered before freeze keeps working after", "[core][ecs][component-registry]")
{
    World world;
    REQUIRE_FALSE(world.ComponentsFrozen());

    Entity a = world.Spawn();
    world.AddComponent(a, Position{1.0f}); // registers Position before freeze

    world.FreezeComponents();
    REQUIRE(world.ComponentsFrozen());

    Entity b = world.Spawn();
    world.AddComponent(b, Position{2.0f}); // Position already known -> succeeds
    REQUIRE(world.GetComponent<Position>(b)->m_x == 2.0f);
    REQUIRE(world.GetComponent<Position>(a)->m_x == 1.0f);

    world.Validate();
}

TEST_CASE("World FreezeComponents: overwriting an already-added component after freeze still stamps the tick", "[core][ecs][component-registry]")
{
    World world;
    Entity a = world.Spawn();
    world.AddComponent(a, Position{1.0f});
    world.FreezeComponents();

    world.AdvanceTick();
    world.AddComponent(a, Position{2.0f}); // overwrite, not a new registration
    REQUIRE(world.GetComponent<Position>(a)->m_x == 2.0f);
    REQUIRE(world.GetComponentMeta<Position>(a)->m_changedTick == world.CurrentTick());

    world.Validate();
}
