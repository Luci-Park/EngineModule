#include <catch2/catch_test_macros.hpp>

#include <engine/core/ecs/Column.hpp>
#include <engine/core/ecs/TypeId.hpp>

#include <type_traits>

using namespace engine;

namespace
{
    struct Position
    {
        float m_x = 0.0f;
    };
}

// precondition Column relies on for its all-or-nothing Push/MoveRowTo (nothrow relocation).
// Column<T> static_asserts this internally; documenting it here so a component that violates
// it fails loudly against the same trait rather than deep inside a template error.
static_assert(std::is_nothrow_move_constructible_v<Position> && std::is_nothrow_move_assignable_v<Position>);

TEST_CASE("Column Push/Get stores value and meta", "[core][ecs][storage]")
{
    Column<Position> column;
    column.Push(Position{1.0f}, ComponentMeta{7, 9});

    REQUIRE(column.Size() == 1);
    REQUIRE(column.Get(0).m_x == 1.0f);
    REQUIRE(column.Meta(0).m_changedTick == 7);
    REQUIRE(column.Meta(0).m_addedTick == 9);
}

TEST_CASE("Column SwapRemove moves the last row's data and meta into the hole", "[core][ecs][storage]")
{
    Column<Position> column;
    column.Push(Position{1.0f}, ComponentMeta{1, 1});
    column.Push(Position{2.0f}, ComponentMeta{2, 2});
    column.Push(Position{3.0f}, ComponentMeta{3, 3});

    column.SwapRemove(0); // last row (index 2) moves into hole 0

    REQUIRE(column.Size() == 2);
    REQUIRE(column.Get(0).m_x == 3.0f);
    REQUIRE(column.Meta(0).m_changedTick == 3);
    REQUIRE(column.Get(1).m_x == 2.0f);
    REQUIRE(column.Meta(1).m_changedTick == 2);
}

TEST_CASE("Column SwapRemove of the last row just pops", "[core][ecs][storage]")
{
    Column<Position> column;
    column.Push(Position{1.0f}, ComponentMeta{1, 1});
    column.Push(Position{2.0f}, ComponentMeta{2, 2});

    column.SwapRemove(1);

    REQUIRE(column.Size() == 1);
    REQUIRE(column.Get(0).m_x == 1.0f);
    REQUIRE(column.Meta(0).m_changedTick == 1);
}

TEST_CASE("Column MoveRowTo appends value+meta to dst and swap-removes from src", "[core][ecs][storage]")
{
    Column<Position> src;
    src.Push(Position{1.0f}, ComponentMeta{1, 1});
    src.Push(Position{2.0f}, ComponentMeta{2, 2});

    Column<Position> dst;
    src.MoveRowTo(0, dst);

    REQUIRE(src.Size() == 1);
    REQUIRE(src.Get(0).m_x == 2.0f); // last row moved into the hole left by row 0

    REQUIRE(dst.Size() == 1);
    REQUIRE(dst.Get(0).m_x == 1.0f);
    REQUIRE(dst.Meta(0).m_changedTick == 1);
}

TEST_CASE("Column ComponentSeq matches TypeIdOf<T>", "[core][ecs][storage]")
{
    Column<Position> column;
    REQUIRE(column.ComponentSeq() == TypeIdOf<Position>().m_seq);
}

TEST_CASE("Column CloneEmpty yields an empty same-seq column", "[core][ecs][storage]")
{
    Column<Position> column;
    column.Push(Position{1.0f});

    std::unique_ptr<IColumn> clone = column.CloneEmpty();
    REQUIRE(clone->Size() == 0);
    REQUIRE(clone->ComponentSeq() == column.ComponentSeq());
}

TEST_CASE("Column keeps data and meta parallel across a sequence of removes", "[core][ecs][storage]")
{
    Column<Position> column;
    for (int i = 0; i < 5; ++i)
    {
        column.Push(Position{static_cast<float>(i)}, ComponentMeta{static_cast<uint32_t>(i), 0});
    }

    // remove indices 1 and then (post-removal) 0, verifying data/meta always agree
    column.SwapRemove(1);
    for (std::size_t row = 0; row < column.Size(); ++row)
    {
        REQUIRE(static_cast<uint32_t>(column.Get(row).m_x) == column.Meta(row).m_changedTick);
    }

    column.SwapRemove(0);
    for (std::size_t row = 0; row < column.Size(); ++row)
    {
        REQUIRE(static_cast<uint32_t>(column.Get(row).m_x) == column.Meta(row).m_changedTick);
    }
}
