#include <catch2/catch_test_macros.hpp>

#include <engine/core/ecs/Entity.hpp>
#include <engine/core/ecs/Mut.hpp>
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

    // fetch without deref -- must not stamp
    {
        auto mut = world.GetComponentMut<Position>(a);
        REQUIRE(mut.has_value());
    }
    REQUIRE(world.GetComponentMeta<Position>(a)->m_changedTick == 1u);

    // deref via operator-> -- must stamp
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
