#include <catch2/catch_test_macros.hpp>

#include <engine/core/ecs/Entity.hpp>
#include <engine/core/ecs/StorageKind.hpp>
#include <engine/core/ecs/World.hpp>

#include <algorithm>
#include <map>
#include <random>
#include <set>
#include <vector>

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

    struct Health
    {
        int m_hp = 0;
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

// --- chunk 1: skeleton + entity lifecycle ----------------------------------

TEST_CASE("World Spawn/Despawn/IsAlive", "[core][ecs][world]")
{
    World world;
    Entity e = world.Spawn();

    REQUIRE_FALSE(e.IsNull());
    REQUIRE(world.IsAlive(e));
    REQUIRE(world.EntityCount() == 1);
    world.Validate();

    world.Despawn(e);
    REQUIRE_FALSE(world.IsAlive(e));
    REQUIRE(world.EntityCount() == 0);
    world.Validate();
}

TEST_CASE("World Despawn relocates the displaced entity", "[core][ecs][world]")
{
    World world;
    Entity a = world.Spawn();
    Entity b = world.Spawn();
    Entity c = world.Spawn();
    world.Validate();

    world.Despawn(a); // b, c stay alive; c (last row) fills a's hole

    REQUIRE_FALSE(world.IsAlive(a));
    REQUIRE(world.IsAlive(b));
    REQUIRE(world.IsAlive(c));
    REQUIRE(world.EntityCount() == 2);
    world.Validate();
}

TEST_CASE("World Despawn on a dead entity is a safe no-op", "[core][ecs][world]")
{
    World world;
    Entity e = world.Spawn();
    world.Despawn(e);

    world.Despawn(e); // second despawn, already dead
    REQUIRE(world.EntityCount() == 0);
    world.Validate();
}

// --- chunk 3: table-component add/remove/get/has + archetype transitions --

TEST_CASE("World AddComponent creates and reuses archetypes", "[core][ecs][world]")
{
    World world;
    Entity a = world.Spawn();
    world.AddComponent(a, Position{1.0f});
    world.AddComponent(a, Velocity{2.0f});
    world.Validate();

    REQUIRE(world.HasComponent<Position>(a));
    REQUIRE(world.HasComponent<Velocity>(a));
    REQUIRE(world.GetComponent<Position>(a)->m_x == 1.0f);
    REQUIRE(world.GetComponent<Velocity>(a)->m_dx == 2.0f);

    // second entity built the same way should land in the same (reused) archetype
    Entity b = world.Spawn();
    world.AddComponent(b, Position{3.0f});
    world.AddComponent(b, Velocity{4.0f});
    world.Validate();

    REQUIRE(world.GetComponent<Position>(b)->m_x == 3.0f);
    REQUIRE(world.GetComponent<Velocity>(b)->m_dx == 4.0f);
    // a's data must be untouched by b's transitions
    REQUIRE(world.GetComponent<Position>(a)->m_x == 1.0f);
    REQUIRE(world.GetComponent<Velocity>(a)->m_dx == 2.0f);
}

TEST_CASE("World RemoveComponent moves to the smaller archetype", "[core][ecs][world]")
{
    World world;
    Entity a = world.Spawn();
    world.AddComponent(a, Position{1.0f});
    world.AddComponent(a, Velocity{2.0f});
    world.AddComponent(a, Health{100});
    world.Validate();

    world.RemoveComponent<Velocity>(a);
    world.Validate();

    REQUIRE_FALSE(world.HasComponent<Velocity>(a));
    REQUIRE(world.GetComponent<Velocity>(a) == nullptr);
    REQUIRE(world.HasComponent<Position>(a));
    REQUIRE(world.HasComponent<Health>(a));
    REQUIRE(world.GetComponent<Position>(a)->m_x == 1.0f);
    REQUIRE(world.GetComponent<Health>(a)->m_hp == 100);
}

TEST_CASE("World AddComponent on an existing T overwrites in place", "[core][ecs][world]")
{
    World world;
    Entity a = world.Spawn();
    world.AddComponent(a, Position{1.0f});
    world.AddComponent(a, Position{9.0f}); // add-existing = overwrite
    world.Validate();

    REQUIRE(world.GetComponent<Position>(a)->m_x == 9.0f);
}

TEST_CASE("World RemoveComponent-absent / Get-absent / Has-absent are safe", "[core][ecs][world]")
{
    World world;
    Entity a = world.Spawn();
    world.AddComponent(a, Position{1.0f});

    world.RemoveComponent<Velocity>(a); // never had it -- no-op
    REQUIRE(world.GetComponent<Velocity>(a) == nullptr);
    REQUIRE_FALSE(world.HasComponent<Velocity>(a));
    world.Validate();
}

TEST_CASE("World component ops on a dead entity are safe no-ops", "[core][ecs][world]")
{
    World world;
    Entity a = world.Spawn();
    world.Despawn(a);

    world.AddComponent(a, Position{1.0f});    // no-op, entity is dead
    world.RemoveComponent<Position>(a);       // no-op
    REQUIRE(world.GetComponent<Position>(a) == nullptr);
    REQUIRE_FALSE(world.HasComponent<Position>(a));
    REQUIRE(world.EntityCount() == 0);
    world.Validate();
}

// --- chunk 4: sparse routing -------------------------------------------------

TEST_CASE("World routes SparseSet components without changing the archetype", "[core][ecs][world]")
{
    World world;
    Entity a = world.Spawn();
    world.AddComponent(a, Position{1.0f}); // table component
    world.Validate();

    world.AddComponent(a, Tag{42}); // sparse component
    world.Validate();

    REQUIRE(world.HasComponent<Tag>(a));
    REQUIRE(world.GetComponent<Tag>(a)->m_value == 42);
    // table component must still be reachable -- sparse add must not have transitioned the entity
    REQUIRE(world.HasComponent<Position>(a));
    REQUIRE(world.GetComponent<Position>(a)->m_x == 1.0f);
}

TEST_CASE("World Despawn removes sparse components too", "[core][ecs][world]")
{
    World world;
    Entity a = world.Spawn();
    world.AddComponent(a, Tag{1});
    world.Despawn(a);
    world.Validate();

    Entity b = world.Spawn();
    world.AddComponent(b, Tag{2});
    REQUIRE_FALSE(world.HasComponent<Tag>(a)); // dead entity -- false regardless
    REQUIRE(world.HasComponent<Tag>(b));
    world.Validate();
}

// --- chunk 5: convenience Spawn(Ts...) ---------------------------------------

TEST_CASE("World Spawn(Ts...) builds the {a,b} archetype directly", "[core][ecs][world]")
{
    World world;
    Entity a = world.Spawn(Position{5.0f}, Velocity{6.0f});
    world.Validate();

    REQUIRE(world.HasComponent<Position>(a));
    REQUIRE(world.HasComponent<Velocity>(a));
    REQUIRE(world.GetComponent<Position>(a)->m_x == 5.0f);
    REQUIRE(world.GetComponent<Velocity>(a)->m_dx == 6.0f);
}

// --- chunk 6: randomized property test ---------------------------------------

TEST_CASE("World property: random spawn/add/remove/despawn preserves invariants", "[core][ecs][world]")
{
    World world;
    std::mt19937 rng(42u);
    std::uniform_int_distribution<int> action(0, 3);

    std::vector<Entity> liveEntities;
    std::map<uint32_t, std::set<int>> shadow; // entity index -> component kinds present (0=Pos,1=Vel,2=Health,3=Tag)

    auto hasKind = [](std::set<int> &kinds, int kind) { return kinds.find(kind) != kinds.end(); };

    for (int i = 0; i < 2000; ++i)
    {
        const int act = liveEntities.empty() ? 0 : action(rng);

        if (act == 0) // spawn
        {
            Entity e = world.Spawn();
            liveEntities.push_back(e);
            shadow[e.m_index] = {};
        }
        else if (act == 1) // add a random component kind
        {
            std::uniform_int_distribution<std::size_t> pick(0, liveEntities.size() - 1);
            Entity e = liveEntities[pick(rng)];
            const int kind = std::uniform_int_distribution<int>(0, 3)(rng);

            switch (kind)
            {
                case 0: world.AddComponent(e, Position{static_cast<float>(i)}); break;
                case 1: world.AddComponent(e, Velocity{static_cast<float>(i)}); break;
                case 2: world.AddComponent(e, Health{i}); break;
                default: world.AddComponent(e, Tag{i}); break;
            }
            shadow[e.m_index].insert(kind);
        }
        else if (act == 2) // remove a random component kind
        {
            std::uniform_int_distribution<std::size_t> pick(0, liveEntities.size() - 1);
            Entity e = liveEntities[pick(rng)];
            const int kind = std::uniform_int_distribution<int>(0, 3)(rng);

            switch (kind)
            {
                case 0: world.RemoveComponent<Position>(e); break;
                case 1: world.RemoveComponent<Velocity>(e); break;
                case 2: world.RemoveComponent<Health>(e); break;
                default: world.RemoveComponent<Tag>(e); break;
            }
            shadow[e.m_index].erase(kind);
        }
        else // despawn
        {
            std::uniform_int_distribution<std::size_t> pick(0, liveEntities.size() - 1);
            std::size_t idx = pick(rng);
            Entity e = liveEntities[idx];

            world.Despawn(e);
            shadow.erase(e.m_index);
            liveEntities[idx] = liveEntities.back();
            liveEntities.pop_back();
        }

        world.Validate();
        REQUIRE(world.EntityCount() == liveEntities.size());

        for (Entity e : liveEntities)
        {
            auto &kinds = shadow[e.m_index];
            REQUIRE(world.HasComponent<Position>(e) == hasKind(kinds, 0));
            REQUIRE(world.HasComponent<Velocity>(e) == hasKind(kinds, 1));
            REQUIRE(world.HasComponent<Health>(e) == hasKind(kinds, 2));
            REQUIRE(world.HasComponent<Tag>(e) == hasKind(kinds, 3));
        }
    }
}
