#include <catch2/catch_test_macros.hpp>

#include <engine/core/ecs/Entity.hpp>
#include <engine/core/ecs/Query.hpp>
#include <engine/core/ecs/QueryFilters.hpp>
#include <engine/core/ecs/StorageKind.hpp>
#include <engine/core/ecs/World.hpp>

#include <algorithm>
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

    struct Frozen
    {
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


static_assert(!IS_FILTER_V<Position>, "bare fetch type is not a filter");
static_assert(!IS_FILTER_V<const Position>, "const fetch type is not a filter");
static_assert(!IS_FILTER_V<Entity>, "Entity is not a filter");
static_assert(IS_FILTER_V<With<Position>>, "With<T> is a filter");
static_assert(IS_FILTER_V<Without<Position>>, "Without<T> is a filter");
static_assert(IS_FILTER_V<Changed<Position>>, "Changed<T> is a filter");
static_assert(IS_FILTER_V<Added<Position>>, "Added<T> is a filter");


TEST_CASE("Query<Entity> alone is valid and yields every live entity", "[core][ecs][query]")
{
    World world;
    Entity a = world.Spawn();
    Entity b = world.Spawn();

    Query<Entity> q(world);
    std::vector<Entity> seen;
    for (auto [e] : q)
        seen.push_back(e);

    REQUIRE(seen.size() == 2);
    REQUIRE(std::find(seen.begin(), seen.end(), a) != seen.end());
    REQUIRE(std::find(seen.begin(), seen.end(), b) != seen.end());
}

TEST_CASE("Query yields Entity / const T& / Mut<T> per param constness; Mut deref stamps, const never does",
          "[core][ecs][query]")
{
    World world;
    Entity a = world.Spawn(Position{1.0f}, Velocity{2.0f});
    world.Validate();

    Query<Entity, const Position, Velocity> q(world);
    std::size_t count = 0;
    for (auto [e, pos, vel] : q)
    {
        REQUIRE(e == a);
        REQUIRE(pos.m_x == 1.0f);
        vel->m_dx += 1.0f; // deref through Mut; stamps
        ++count;
    }
    REQUIRE(count == 1);
    REQUIRE(world.GetComponent<Velocity>(a)->m_dx == 3.0f);
    REQUIRE(world.GetComponentMeta<Velocity>(a)->m_changedTick == world.CurrentTick());

    // const Position must never stamp; changed still equals added (never touched since add)
    const ComponentMeta *posMeta = world.GetComponentMeta<Position>(a);
    REQUIRE(posMeta->m_changedTick == posMeta->m_addedTick);
}

TEST_CASE("Query visits entities across multiple archetypes exactly once", "[core][ecs][query]")
{
    World world;
    Entity a = world.Spawn(Position{1.0f});
    Entity b = world.Spawn(Position{2.0f}, Velocity{3.0f});
    Entity c = world.Spawn(Position{4.0f}, Velocity{5.0f}, Health{6});
    world.Validate();

    Query<Entity, const Position> q(world);
    std::vector<Entity> seen;
    for (auto [e, pos] : q)
    {
        (void)pos;
        seen.push_back(e);
    }

    REQUIRE(seen.size() == 3);
    REQUIRE(std::find(seen.begin(), seen.end(), a) != seen.end());
    REQUIRE(std::find(seen.begin(), seen.end(), b) != seen.end());
    REQUIRE(std::find(seen.begin(), seen.end(), c) != seen.end());
}

TEST_CASE("Query matching nothing iterates zero times", "[core][ecs][query]")
{
    World world;
    world.Spawn(Position{1.0f});

    Query<const Velocity> q(world);
    REQUIRE(q.Count() == 0);
}

TEST_CASE("Query includes the empty archetype (component-less entities)", "[core][ecs][query]")
{
    World world;
    Entity a = world.Spawn(); // never gets a table component; stays in the empty archetype
    Entity b = world.Spawn(Position{1.0f});
    world.Validate();

    Query<Entity> q(world);
    REQUIRE(q.Count() == 2);

    Query<Entity, Without<Position>> withoutQ(world);
    bool foundA = false;
    for (auto [e] : withoutQ)
        if (e == a)
            foundA = true;
    REQUIRE(foundA);
    (void)b;
}

TEST_CASE("Query re-matches archetypes created after it was built", "[core][ecs][query]")
{
    World world;
    Entity a = world.Spawn(Position{1.0f});

    Query<Entity, const Position> q(world);          // built while only {Position} exists
    Entity b = world.Spawn(Position{2.0f}, Velocity{3.0f}); // creates a NEW archetype {Position,Velocity}
    world.Validate();

    // b has Position, so it matches; a Query holding a stale archetype list would drop it
    REQUIRE(q.Count() == 2);

    std::vector<Entity> seen;
    for (auto [e, pos] : q)
    {
        (void)pos;
        seen.push_back(e);
    }
    REQUIRE(seen.size() == 2);
    REQUIRE(std::find(seen.begin(), seen.end(), a) != seen.end());
    REQUIRE(std::find(seen.begin(), seen.end(), b) != seen.end());

    // and again for a sparse-only query, whose match list is every archetype
    Query<Entity, const Tag> sparseQ(world);
    world.AddComponent(a, Tag{7});
    Entity c = world.Spawn(Health{1}); // yet another new archetype {Health}
    world.AddComponent(c, Tag{8});
    world.Validate();

    REQUIRE(sparseQ.Count() == 2);
}

// an exhausted iterator must stay equal to end() even if the match list grows afterwards.
// end() is re-evaluated every pass of `for (it = q.begin(); it != q.end(); ++it)`, so a
// sentinel derived from the live m_matched.size() would step past a parked iterator and
// resurrect it onto a row RowMatches never validated.
TEST_CASE("Query end() sentinel is stable across archetype creation", "[core][ecs][query]")
{
    World world;
    world.Spawn(Position{1.0f});

    Query<Entity, const Position> q(world);
    auto it = q.begin();
    auto stop = q.end();

    ++it; // only one match, so this parks the iterator at end
    REQUIRE_FALSE(it != stop);

    world.Spawn(Health{5}); // new archetype; does not match, but grows the archetype vector
    world.Spawn(Position{2.0f}, Velocity{3.0f}); // new archetype that DOES match
    world.Validate();

    REQUIRE_FALSE(it != q.end()); // parked iterator must not come back to life
}

TEST_CASE("Query Count() agrees with a manual range-for tally", "[core][ecs][query]")
{
    World world;
    world.Spawn(Position{1.0f});
    world.Spawn(Position{2.0f}, Velocity{1.0f});
    world.Spawn(Position{3.0f}, Health{5});
    world.Validate();

    std::size_t manual = 0;
    {
        Query<const Position> q(world);
        for (auto [pos] : q)
        {
            (void)pos;
            ++manual;
        }
    }

    Query<const Position> q2(world);
    REQUIRE(q2.Count() == manual);
    REQUIRE(manual == 3);
}


TEST_CASE("Query With/Without filter on table components", "[core][ecs][query]")
{
    World world;
    Entity a = world.Spawn(Position{1.0f}, Frozen{});
    Entity b = world.Spawn(Position{2.0f});
    world.Validate();

    {
        Query<Entity, const Position, With<Frozen>> q(world);
        REQUIRE(q.Count() == 1);
        for (auto [e, pos] : q)
        {
            (void)pos;
            REQUIRE(e == a);
        }
    }
    {
        Query<Entity, const Position, Without<Frozen>> q(world);
        REQUIRE(q.Count() == 1);
        for (auto [e, pos] : q)
        {
            (void)pos;
            REQUIRE(e == b);
        }
    }
}

TEST_CASE("Query combines With and Without in a single query", "[core][ecs][query]")
{
    World world;

    Entity a = world.Spawn(Position{1.0f});
    world.AddComponent(a, Frozen{}); // has Frozen only

    Entity b = world.Spawn(Position{2.0f});
    world.AddComponent(b, Frozen{});
    world.Validate();

    Query<Entity, With<Frozen>, Without<Velocity>> q(world);
    std::vector<Entity> seen;
    for (auto [e] : q)
        seen.push_back(e);

    REQUIRE(seen.size() == 2);
    REQUIRE(std::find(seen.begin(), seen.end(), a) != seen.end());
    REQUIRE(std::find(seen.begin(), seen.end(), b) != seen.end());

    // now give b a Velocity; With<Frozen> + Without<Velocity> must drop b, keep a
    world.AddComponent(b, Velocity{1.0f});
    Query<Entity, With<Frozen>, Without<Velocity>> narrowed(world);
    REQUIRE(narrowed.Count() == 1);
    for (auto [e] : narrowed)
        REQUIRE(e == a);
}

TEST_CASE("Query Without includes entities in the empty archetype", "[core][ecs][query]")
{
    World world;
    Entity a = world.Spawn(); // no components at all
    world.Validate();

    Query<Entity, Without<Frozen>> q(world);
    bool found = false;
    for (auto [e] : q)
        if (e == a)
            found = true;
    REQUIRE(found);
}


TEST_CASE("Query sparse fetch yields correct value and Mut deref stamps", "[core][ecs][query]")
{
    World world;
    Entity a = world.Spawn();
    world.AddComponent(a, Tag{5});
    world.Validate();

    Query<Entity, Tag> q(world);
    for (auto [e, tag] : q)
    {
        REQUIRE(e == a);
        tag->m_value = 9;
    }
    REQUIRE(world.GetComponent<Tag>(a)->m_value == 9);
    REQUIRE(world.GetComponentMeta<Tag>(a)->m_changedTick == world.CurrentTick());
}

TEST_CASE("Query sparse With/Without", "[core][ecs][query]")
{
    World world;
    Entity a = world.Spawn();
    world.AddComponent(a, Tag{1});
    Entity b = world.Spawn(); // no Tag
    world.Validate();

    {
        Query<Entity, With<Tag>> q(world);
        REQUIRE(q.Count() == 1);
        for (auto [e] : q)
            REQUIRE(e == a);
    }
    {
        Query<Entity, Without<Tag>> q(world);
        std::size_t count = 0;
        bool sawB = false;
        for (auto [e] : q)
        {
            ++count;
            if (e == b)
                sawB = true;
        }
        REQUIRE(count == 1);
        REQUIRE(sawB);
    }
}

TEST_CASE("Query on a never-created sparse storage: fetch matches nothing, Without matches everything",
          "[core][ecs][query]")
{
    World world;
    world.Spawn(Position{1.0f});
    world.Spawn(Position{2.0f});
    world.Validate();
    // Tag storage never created; no entity in this World ever got a Tag

    {
        Query<const Tag> q(world);
        REQUIRE(q.Count() == 0);
    }
    {
        Query<Entity, With<Tag>> q(world);
        REQUIRE(q.Count() == 0);
    }
    {
        Query<Entity, Without<Tag>> q(world);
        REQUIRE(q.Count() == 2);
    }
}

TEST_CASE("Query fetches a Table component and a sparse component together on one entity", "[core][ecs][query]")
{
    World world;
    Entity a = world.Spawn(Position{1.0f});
    world.AddComponent(a, Tag{7});
    world.Validate();

    Query<Entity, const Position, const Tag> q(world);
    REQUIRE(q.Count() == 1);
    for (auto [e, pos, tag] : q)
    {
        REQUIRE(e == a);
        REQUIRE(pos.m_x == 1.0f);
        REQUIRE(tag.m_value == 7);
    }
}


TEST_CASE("Query Changed/Added with explicit lastRunTick, including the '>' regression", "[core][ecs][query]")
{
    World world;
    Entity a = world.Spawn();
    world.AddComponent(a, Position{1.0f}); // tick 1; added==changed==1

    // lastRunTick = 0 matches everything (first-run semantics)
    {
        Query<Entity, const Position, Changed<Position>> q(world, 0);
        REQUIRE(q.Count() == 1);
    }
    {
        Query<Entity, const Position, Added<Position>> q(world, 0);
        REQUIRE(q.Count() == 1);
    }

    world.AdvanceTick();                   // tick 2
    world.AddComponent(a, Position{9.0f}); // overwrite; changed=2, added stays 1

    {
        Query<Entity, const Position, Added<Position>> q(world, 1);
        REQUIRE(q.Count() == 0); // added==1, not > 1; overwrite must not re-fire Added
    }
    {
        Query<Entity, const Position, Changed<Position>> q(world, 1);
        REQUIRE(q.Count() == 1); // changed==2 > 1
    }

    // the '>'-vs-'>=' regression: stamped at tick 2, queried with lastRunTick == 2 -> must NOT
    // match. '>=' would incorrectly match here (double-fire); this is what pins '>'.
    {
        Query<Entity, const Position, Changed<Position>> q(world, 2);
        REQUIRE(q.Count() == 0);
    }
}

TEST_CASE("Query Changed discriminates the mutated entity from an untouched one", "[core][ecs][query]")
{
    World world;
    Entity a = world.Spawn(Position{1.0f});
    Entity b = world.Spawn(Position{2.0f});
    world.Validate();

    world.AdvanceTick(); // tick 2
    {
        auto mut = world.GetComponentMut<Position>(a);
        (*mut)->m_x = 100.0f; // only a is mutated
    }

    Query<Entity, Changed<Position>> q(world, 1);
    REQUIRE(q.Count() == 1);
    for (auto [e] : q)
        REQUIRE(e == a);
}

TEST_CASE("Query Added implies Changed for a freshly-added component", "[core][ecs][query]")
{
    World world;
    Entity a = world.Spawn();
    world.AdvanceTick();           // tick 2
    world.AddComponent(a, Tag{1}); // added==changed==2 (sparse)

    Query<Entity, Added<Tag>> added(world, 1);
    REQUIRE(added.Count() == 1);
    Query<Entity, Changed<Tag>> changed(world, 1);
    REQUIRE(changed.Count() == 1);
}

TEST_CASE("Query Changed<T> never matches an entity lacking T (presence implied)", "[core][ecs][query]")
{
    World world;
    Entity a = world.Spawn(Position{1.0f});
    world.Spawn(); // no Position
    world.Validate();

    Query<Entity, Changed<Position>> q(world, 0);
    REQUIRE(q.Count() == 1);
    for (auto [e] : q)
        REQUIRE(e == a);
}

TEST_CASE("Query Mut deref is visible to a later Changed query; Mut::Read() is not", "[core][ecs][query]")
{
    World world;
    world.Spawn(Position{1.0f}); // tick 1
    world.AdvanceTick();         // tick 2

    {
        Query<Position> q(world); // mutable -> Mut<Position>
        for (auto [pos] : q)
            pos->m_x = 42.0f; // deref; stamps changed=2
    }
    {
        Query<Entity, Changed<Position>> changedQ(world, 1);
        REQUIRE(changedQ.Count() == 1); // changed(2) > lastRun(1)
    }

    world.AdvanceTick(); // tick 3
    {
        Query<Position> q(world);
        for (auto [pos] : q)
            (void)pos.Read().m_x; // read-only; must NOT stamp
    }
    {
        Query<Entity, Changed<Position>> changedQ(world, 2);
        REQUIRE(changedQ.Count() == 0); // still stamped at 2, not > 2
    }
}
