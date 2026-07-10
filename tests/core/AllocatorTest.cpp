#include <catch2/catch_test_macros.hpp>

#include <engine/core/memory/LinearAllocator.hpp>
#include <engine/core/memory/PoolAllocator.hpp>
#include <engine/core/memory/Memory.hpp>

#include <cstdint>

using namespace engine;

TEST_CASE("linear allocator bumps and respects alignment", "[core][memory]")
{
    LinearAllocator arena(256);

    void* a = arena.Allocate(10, 16);
    REQUIRE(a != nullptr);
    REQUIRE((reinterpret_cast<std::uintptr_t>(a) % 16) == 0);

    void* b = arena.Allocate(10, 16);
    REQUIRE(b != nullptr);
    REQUIRE(b != a);
}

TEST_CASE("linear allocator returns nullptr on exhaustion", "[core][memory]")
{
    LinearAllocator arena(16);
    REQUIRE(arena.Allocate(16, 1) != nullptr);
    REQUIRE(arena.Allocate(1, 1) == nullptr);  // no room left
}

TEST_CASE("linear allocator reset reclaims the arena", "[core][memory]")
{
    LinearAllocator arena(16);
    REQUIRE(arena.Allocate(16, 1) != nullptr);
    REQUIRE(arena.Allocate(1, 1) == nullptr);

    arena.Reset();
    REQUIRE(arena.Allocate(16, 1) != nullptr);
}

TEST_CASE("pool allocator hands out and recycles fixed blocks", "[core][memory]")
{
    PoolAllocator pool(32, 4);

    void* a = pool.Allocate(32);
    void* b = pool.Allocate(32);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(a != b);
    REQUIRE(pool.FreeCount() == 2);

    pool.Deallocate(a);
    REQUIRE(pool.FreeCount() == 3);

    void* c = pool.Allocate(32);
    REQUIRE(c == a);  // reused the freed block
}

TEST_CASE("pool allocator returns nullptr on exhaustion", "[core][memory]")
{
    PoolAllocator pool(16, 2);
    REQUIRE(pool.Allocate(16) != nullptr);
    REQUIRE(pool.Allocate(16) != nullptr);
    REQUIRE(pool.Allocate(16) == nullptr);  // exhausted
}

TEST_CASE("pool allocator rejects a request that doesn't fit the block", "[core][memory]")
{
    PoolAllocator pool(16, 2, 8);
    REQUIRE(pool.Allocate(32) == nullptr);  // too big
    REQUIRE(pool.Allocate(8, 64) == nullptr);  // over-aligned
}

TEST_CASE("tracked allocation updates and reverts stats", "[core][memory]")
{
    const auto before = Stats();

    struct Payload { int values[16]; };
    Payload* p = Allocate<Payload>(MemoryTag::General);
    REQUIRE(p != nullptr);

    const auto during = Stats();
    REQUIRE(during.currentBytes >= before.currentBytes + sizeof(Payload));
    REQUIRE(during.allocationCount == before.allocationCount + 1);

    Delete(p);

    const auto after = Stats();
    REQUIRE(after.currentBytes == before.currentBytes);
    REQUIRE(after.allocationCount == before.allocationCount);
}

TEST_CASE("per-tag stats attribute to the requested tag", "[core][memory]")
{
    const auto tagIdx = static_cast<std::size_t>(MemoryTag::Assets);
    const auto before = Stats();

    struct Chunk { char data[64]; };
    Chunk* c = Allocate<Chunk>(MemoryTag::Assets);
    REQUIRE(c != nullptr);

    REQUIRE(Stats().perTagBytes[tagIdx] >= before.perTagBytes[tagIdx] + sizeof(Chunk));

    Delete(c);
    REQUIRE(Stats().perTagBytes[tagIdx] == before.perTagBytes[tagIdx]);
}

TEST_CASE("all-freed path reports zero live allocations", "[core][memory]")
{
    struct Temp { int x; };
    Temp* t = Allocate<Temp>(MemoryTag::General);
    Delete(t);

    ReportLeaks();  // 0 leaks -> assert passes, safe to call
    REQUIRE(LiveAllocationCount() == 0);
}

TEST_CASE("small-alignment types allocate without misalignment", "[core][memory]")
{
    // alignof(char)==1, alignof(int)==4 -> both smaller than alignof(AllocHeader).
    // header must still land aligned (regression for the effAlign fix).
    struct Bytes { char data[7]; };
    Bytes* b = Allocate<Bytes>(MemoryTag::General);
    int* i = Allocate<int>(MemoryTag::General, 42);
    REQUIRE(b != nullptr);
    REQUIRE(i != nullptr);
    REQUIRE(*i == 42);
    Delete(b);
    Delete(i);
}

#ifndef NDEBUG
TEST_CASE("unfreed allocation is visible to the leak tracker", "[core][memory]")
{
    // catch_discover_tests runs each TEST_CASE in its own process, so this
    // deliberate non-free doesn't poison any other test. LiveAllocationCount() is
    // the non-fatal query -> no ENGINE_ASSERT/break here (that's ReportLeaks').
    const auto before = LiveAllocationCount();

    struct Leaked { char data[64]; };
    Leaked* leaked = Allocate<Leaked>(MemoryTag::General);
    REQUIRE(leaked != nullptr);

    REQUIRE(LiveAllocationCount() == before + 1);
}
#endif
