#include <catch2/catch_test_macros.hpp>

#include <engine/core/ecs/StorageKind.hpp>

using namespace engine;

namespace
{
    struct DefaultComponent
    {
    };

    struct SparseComponent
    {
    };
}

namespace engine
{
    template<>
    struct ComponentStorageKind<SparseComponent>
    {
        static constexpr StorageKind VALUE = StorageKind::SparseSet;
    };
}

static_assert(ComponentStorageKind<DefaultComponent>::VALUE == StorageKind::Table);
static_assert(ComponentStorageKind<SparseComponent>::VALUE == StorageKind::SparseSet);

TEST_CASE("ComponentStorageKind defaults to Table and honors opt-in specialization", "[core][ecs][storage]")
{
    REQUIRE(ComponentStorageKind<DefaultComponent>::VALUE == StorageKind::Table);
    REQUIRE(ComponentStorageKind<SparseComponent>::VALUE == StorageKind::SparseSet);
}
