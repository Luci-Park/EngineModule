#include <catch2/catch_test_macros.hpp>

#include <engine/core/ecs/Entity.hpp>
#include <engine/core/ecs/Mut.hpp>
#include <engine/core/ecs/StorageKind.hpp>
#include <engine/core/ecs/World.hpp>

#include <type_traits>

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

    // sparse-routed component
    struct Tag
    {
        int m_value = 0;
    };
}

template <>
struct engine::ComponentStorageKind<Tag>
{
    static constexpr StorageKind VALUE = StorageKind::SparseSet;
};

TEST_CASE("World tick clock starts at 1 and advances by 1", "[core][ecs][change-detection]")
{
    World world;
    REQUIRE(world.CurrentTick() == 1u);

    world.AdvanceTick();
    REQUIRE(world.CurrentTick() == 2u);

    world.AdvanceTick();
    REQUIRE(world.CurrentTick() == 3u);
}

TEST_CASE("World AddComponent stamps added==changed==CurrentTick (table)", "[core][ecs][change-detection]")
{
    World world;
    Entity a = world.Spawn();

    world.AdvanceTick(); // tick 2
    world.AddComponent(a, Position{1.0f});

    const ComponentMeta *meta = world.GetComponentMeta<Position>(a);
    REQUIRE(meta != nullptr);
    REQUIRE(meta->m_addedTick == 2u);
    REQUIRE(meta->m_changedTick == 2u);
}

TEST_CASE("World AddComponent stamps added==changed==CurrentTick (sparse)", "[core][ecs][change-detection]")
{
    World world;
    Entity a = world.Spawn();

    world.AdvanceTick(); // tick 2
    world.AddComponent(a, Tag{1});

    const ComponentMeta *meta = world.GetComponentMeta<Tag>(a);
    REQUIRE(meta != nullptr);
    REQUIRE(meta->m_addedTick == 2u);
    REQUIRE(meta->m_changedTick == 2u);
}

TEST_CASE("World overwrite bumps changed but preserves added (table)", "[core][ecs][change-detection]")
{
    World world;
    Entity a = world.Spawn();

    world.AddComponent(a, Position{1.0f}); // tick 1
    world.AdvanceTick();                   // tick 2
    world.AdvanceTick();                   // tick 3
    world.AddComponent(a, Position{9.0f}); // overwrite at tick 3

    const ComponentMeta *meta = world.GetComponentMeta<Position>(a);
    REQUIRE(meta != nullptr);
    REQUIRE(meta->m_addedTick == 1u);
    REQUIRE(meta->m_changedTick == 3u);
    REQUIRE(world.GetComponent<Position>(a)->m_x == 9.0f);
}

TEST_CASE("World overwrite bumps changed but preserves added (sparse)", "[core][ecs][change-detection]")
{
    World world;
    Entity a = world.Spawn();

    world.AddComponent(a, Tag{1}); // tick 1
    world.AdvanceTick();           // tick 2
    world.AdvanceTick();           // tick 3
    world.AddComponent(a, Tag{9}); // overwrite at tick 3

    const ComponentMeta *meta = world.GetComponentMeta<Tag>(a);
    REQUIRE(meta != nullptr);
    REQUIRE(meta->m_addedTick == 1u);
    REQUIRE(meta->m_changedTick == 3u);
    REQUIRE(world.GetComponent<Tag>(a)->m_value == 9);
}

TEST_CASE("World GetComponentMut deref stamps changed; fetch alone does not (table)", "[core][ecs][change-detection]")
{
    World world;
    Entity a = world.Spawn();
    world.AddComponent(a, Position{1.0f}); // tick 1

    world.AdvanceTick(); // tick 2

    // fetch without deref; must not stamp
    {
        auto mut = world.GetComponentMut<Position>(a);
        REQUIRE(mut.has_value());
    }
    REQUIRE(world.GetComponentMeta<Position>(a)->m_changedTick == 1u);

    // deref via operator->; must stamp
    {
        auto mut = world.GetComponentMut<Position>(a);
        REQUIRE(mut.has_value());
        (*mut)->m_x = 5.0f;
    }
    REQUIRE(world.GetComponentMeta<Position>(a)->m_changedTick == 2u);
    REQUIRE(world.GetComponent<Position>(a)->m_x == 5.0f);
}

TEST_CASE("World GetComponentMut deref stamps changed; fetch alone does not (sparse)", "[core][ecs][change-detection]")
{
    World world;
    Entity a = world.Spawn();
    world.AddComponent(a, Tag{1}); // tick 1

    world.AdvanceTick(); // tick 2

    {
        auto mut = world.GetComponentMut<Tag>(a);
        REQUIRE(mut.has_value());
    }
    REQUIRE(world.GetComponentMeta<Tag>(a)->m_changedTick == 1u);

    {
        auto mut = world.GetComponentMut<Tag>(a);
        REQUIRE(mut.has_value());
        (*(*mut)).m_value = 7;
    }
    REQUIRE(world.GetComponentMeta<Tag>(a)->m_changedTick == 2u);
    REQUIRE(world.GetComponent<Tag>(a)->m_value == 7);
}

TEST_CASE("World archetype transition preserves surviving component meta", "[core][ecs][change-detection]")
{
    World world;
    Entity a = world.Spawn();

    world.AddComponent(a, Position{1.0f}); // A added at tick 1

    world.AdvanceTick(); // tick 2
    world.AdvanceTick(); // tick 3
    world.AdvanceTick(); // tick 4
    world.AdvanceTick(); // tick 5
    world.AddComponent(a, Velocity{2.0f}); // forces {A} -> {A,B} move, B added at tick 5

    const ComponentMeta *posMeta = world.GetComponentMeta<Position>(a);
    const ComponentMeta *velMeta = world.GetComponentMeta<Velocity>(a);
    REQUIRE(posMeta != nullptr);
    REQUIRE(velMeta != nullptr);
    REQUIRE(posMeta->m_addedTick == 1u);
    REQUIRE(posMeta->m_changedTick == 1u); // untouched by the transition
    REQUIRE(velMeta->m_addedTick == 5u);
    REQUIRE(velMeta->m_changedTick == 5u);

    world.AdvanceTick(); // tick 6
    world.RemoveComponent<Velocity>(a); // forces {A,B} -> {A} move

    const ComponentMeta *posMetaAfterRemove = world.GetComponentMeta<Position>(a);
    REQUIRE(posMetaAfterRemove != nullptr);
    REQUIRE(posMetaAfterRemove->m_addedTick == 1u);
    REQUIRE(posMetaAfterRemove->m_changedTick == 1u); // still untouched
}

TEST_CASE("World table and sparse components stamp independently", "[core][ecs][change-detection]")
{
    World world;
    Entity a = world.Spawn();

    world.AddComponent(a, Position{1.0f}); // tick 1
    world.AdvanceTick();                   // tick 2
    world.AddComponent(a, Tag{1});         // tick 2

    REQUIRE(world.GetComponentMeta<Position>(a)->m_addedTick == 1u);
    REQUIRE(world.GetComponentMeta<Tag>(a)->m_addedTick == 2u);

    world.AdvanceTick(); // tick 3
    auto mut = world.GetComponentMut<Tag>(a);
    (*mut)->m_value = 42; // stamp Tag's changed tick only

    REQUIRE(world.GetComponentMeta<Tag>(a)->m_changedTick == 3u);
    REQUIRE(world.GetComponentMeta<Position>(a)->m_changedTick == 1u); // untouched
}

TEST_CASE("World GetComponentMut/GetComponentMeta on dead or absent are safe", "[core][ecs][change-detection]")
{
    World world;
    Entity a = world.Spawn();
    world.AddComponent(a, Position{1.0f});

    // absent component
    REQUIRE_FALSE(world.GetComponentMut<Velocity>(a).has_value());
    REQUIRE(world.GetComponentMeta<Velocity>(a) == nullptr);
    REQUIRE_FALSE(world.GetComponentMut<Tag>(a).has_value());
    REQUIRE(world.GetComponentMeta<Tag>(a) == nullptr);

    // dead entity
    world.Despawn(a);
    REQUIRE_FALSE(world.GetComponentMut<Position>(a).has_value());
    REQUIRE(world.GetComponentMeta<Position>(a) == nullptr);
}


TEST_CASE("World GetComponent returns const T*, so an untracked write is a compile error", "[core][ecs][change-detection]")
{
    World world;
    Entity a = world.Spawn();
    world.AddComponent(a, Position{1.0f});

    static_assert(std::is_same_v<decltype(world.GetComponent<Position>(a)), const Position *>,
                  "GetComponent<T> must return const T*; no untracked mutable accessor");
    REQUIRE(world.GetComponent<Position>(a)->m_x == 1.0f);
}

TEST_CASE("World StructuralVersion bumps on Spawn/Despawn/new-Add/Remove, not on overwrite/no-op", "[core][ecs][change-detection]")
{
    World world;

    const uint32_t v0 = world.StructuralVersion();
    Entity a = world.Spawn();
    REQUIRE(world.StructuralVersion() != v0); // Spawn always bumps

    const uint32_t v1 = world.StructuralVersion();
    world.AddComponent(a, Position{1.0f}); // new table component; archetype transition
    REQUIRE(world.StructuralVersion() != v1);

    const uint32_t v2 = world.StructuralVersion();
    world.AddComponent(a, Position{2.0f}); // overwrite; no structural change
    REQUIRE(world.StructuralVersion() == v2);

    const uint32_t v3 = world.StructuralVersion();
    world.AddComponent(a, Tag{1}); // new sparse component
    REQUIRE(world.StructuralVersion() != v3);

    const uint32_t v4 = world.StructuralVersion();
    world.AddComponent(a, Tag{2}); // sparse overwrite; no structural change
    REQUIRE(world.StructuralVersion() == v4);

    const uint32_t v5 = world.StructuralVersion();
    world.RemoveComponent<Velocity>(a); // remove-absent; no-op
    REQUIRE(world.StructuralVersion() == v5);

    const uint32_t v6 = world.StructuralVersion();
    world.RemoveComponent<Tag>(a); // actually removes; structural
    REQUIRE(world.StructuralVersion() != v6);

    const uint32_t v7 = world.StructuralVersion();
    world.RemoveComponent<Position>(a); // actually removes; structural
    REQUIRE(world.StructuralVersion() != v7);

    world.Despawn(a);
    Entity dead = a;
    const uint32_t v8 = world.StructuralVersion();
    world.AddComponent(dead, Position{1.0f}); // dead-entity no-op
    REQUIRE(world.StructuralVersion() == v8);
    world.RemoveComponent<Position>(dead); // dead-entity no-op
    REQUIRE(world.StructuralVersion() == v8);
}

TEST_CASE("Mut::Read() returns the value without stamping; operator*/-> on the same Mut still stamp", "[core][ecs][change-detection]")
{
    World world;
    Entity a = world.Spawn();
    world.AddComponent(a, Position{1.0f}); // tick 1

    world.AdvanceTick(); // tick 2
    {
        auto mut = world.GetComponentMut<Position>(a);
        REQUIRE(mut.has_value());
        REQUIRE(mut->Read().m_x == 1.0f);
    }
    REQUIRE(world.GetComponentMeta<Position>(a)->m_changedTick == 1u); // Read() must not stamp

    {
        auto mut = world.GetComponentMut<Position>(a);
        (*mut)->m_x = 5.0f; // operator-> still stamps
    }
    REQUIRE(world.GetComponentMeta<Position>(a)->m_changedTick == 2u);
}


TEST_CASE("World Mut deref preserves m_addedTick (DEFERRED #3)", "[core][ecs][change-detection]")
{
    World world;
    Entity a = world.Spawn();
    world.AddComponent(a, Position{1.0f}); // added==changed==1

    world.AdvanceTick(); // tick 2
    {
        auto mut = world.GetComponentMut<Position>(a);
        (*mut)->m_x = 5.0f; // deref; bumps changed only
    }

    const ComponentMeta *meta = world.GetComponentMeta<Position>(a);
    REQUIRE(meta->m_addedTick == 1u);   // must survive the deref
    REQUIRE(meta->m_changedTick == 2u);
}

TEST_CASE("World overwritten-then-transitioned meta survives the move (DEFERRED #4)", "[core][ecs][change-detection]")
{
    World world;
    Entity a = world.Spawn();
    world.AddComponent(a, Position{1.0f}); // A added @1

    world.AdvanceTick();                   // tick 2
    world.AdvanceTick();                   // tick 3
    world.AddComponent(a, Position{9.0f}); // A overwritten @3; changed=3, added still 1

    world.AdvanceTick();                    // tick 4
    world.AddComponent(a, Velocity{2.0f}); // forces {A} -> {A,B} transition

    const ComponentMeta *posMeta = world.GetComponentMeta<Position>(a);
    REQUIRE(posMeta->m_addedTick == 1u);
    REQUIRE(posMeta->m_changedTick == 3u); // the MODIFIED tick, not the added tick; must survive the move
}
