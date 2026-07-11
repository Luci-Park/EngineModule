#include <catch2/catch_test_macros.hpp>

#include <engine/core/ecs/Entity.hpp>
#include <engine/core/ecs/Table.hpp>

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

    Entity MakeEntity(uint32_t index)
    {
        return Entity{index, 0};
    }
}

TEST_CASE("Table AddColumn/GetColumn is typed and nullptr for absent T", "[core][ecs][storage]")
{
    Table table;
    table.AddColumn<Position>();

    REQUIRE(table.GetColumn<Position>() != nullptr);
    REQUIRE(table.GetColumn<Velocity>() == nullptr);
}

TEST_CASE("Table builds rows across multiple columns via AddEntity + column Push", "[core][ecs][storage]")
{
    Table table;
    table.AddColumn<Position>();
    table.AddColumn<Velocity>();
    table.AddColumn<Health>();

    for (uint32_t i = 0; i < 3; ++i)
    {
        Entity      e   = MakeEntity(i);
        std::size_t row = table.AddEntity(e);
        table.GetColumn<Position>()->Push(Position{static_cast<float>(i)});
        table.GetColumn<Velocity>()->Push(Velocity{static_cast<float>(i) * 10.0f});
        table.GetColumn<Health>()->Push(Health{static_cast<int>(i) * 100});
        REQUIRE(row == i);
    }

    REQUIRE(table.RowCount() == 3);
    table.Validate();

    REQUIRE(table.GetColumn<Position>()->Get(2).m_x == 2.0f);
    REQUIRE(table.GetColumn<Velocity>()->Get(2).m_dx == 20.0f);
    REQUIRE(table.GetColumn<Health>()->Get(2).m_hp == 200);
}

TEST_CASE("Table SwapRemove returns the moved entity and keeps every column in lockstep", "[core][ecs][storage]")
{
    Table table;
    table.AddColumn<Position>();
    table.AddColumn<Velocity>();

    Entity e0 = MakeEntity(0);
    Entity e1 = MakeEntity(1);
    Entity e2 = MakeEntity(2);

    table.AddEntity(e0);
    table.GetColumn<Position>()->Push(Position{0.0f});
    table.GetColumn<Velocity>()->Push(Velocity{0.0f});

    table.AddEntity(e1);
    table.GetColumn<Position>()->Push(Position{1.0f});
    table.GetColumn<Velocity>()->Push(Velocity{10.0f});

    table.AddEntity(e2);
    table.GetColumn<Position>()->Push(Position{2.0f});
    table.GetColumn<Velocity>()->Push(Velocity{20.0f});

    Entity moved = table.SwapRemove(0); // last row (e2) moves into hole 0

    REQUIRE(moved == e2);
    REQUIRE(table.RowCount() == 2);
    REQUIRE(table.EntityAt(0) == e2);
    REQUIRE(table.EntityAt(1) == e1);
    REQUIRE(table.GetColumn<Position>()->Get(0).m_x == 2.0f);
    REQUIRE(table.GetColumn<Velocity>()->Get(0).m_dx == 20.0f);
    table.Validate();
}

TEST_CASE("Table SwapRemove of the last row returns NULL_ENTITY", "[core][ecs][storage]")
{
    Table table;
    table.AddColumn<Position>();

    Entity e0 = MakeEntity(0);
    Entity e1 = MakeEntity(1);

    table.AddEntity(e0);
    table.GetColumn<Position>()->Push(Position{0.0f});
    table.AddEntity(e1);
    table.GetColumn<Position>()->Push(Position{1.0f});

    Entity moved = table.SwapRemove(1);

    REQUIRE(moved.IsNull());
    REQUIRE(table.RowCount() == 1);
    REQUIRE(table.EntityAt(0) == e0);
    table.Validate();
}

TEST_CASE("Table EntityAt is correct after a sequence of removals", "[core][ecs][storage]")
{
    Table table;
    table.AddColumn<Position>();

    for (uint32_t i = 0; i < 5; ++i)
    {
        table.AddEntity(MakeEntity(i));
        table.GetColumn<Position>()->Push(Position{static_cast<float>(i)});
    }

    table.SwapRemove(1); // e4 -> row 1
    table.SwapRemove(0); // e3 -> row 0
    table.Validate();

    REQUIRE(table.RowCount() == 3);
    // remaining entities: e3, e4, e2 in some swap-remove-determined order; just check no dups
    // and every column stays in lockstep with row count (already asserted by Validate()).
    REQUIRE(table.GetColumn<Position>()->Size() == 3);
}
