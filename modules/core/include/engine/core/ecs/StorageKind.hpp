#pragma once

namespace engine
{
    enum class StorageKind
    {
        Table,
        SparseSet,
    };

    // opt-in per component elsewhere:
    //   template<> struct ComponentStorageKind<Velocity> { static constexpr StorageKind VALUE = StorageKind::SparseSet; };
    template<typename T>
    struct ComponentStorageKind
    {
        static constexpr StorageKind VALUE = StorageKind::Table; // default
    };
}
