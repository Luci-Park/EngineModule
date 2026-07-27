/**
 * @file StorageKind.hpp
 * @author sumin.park
 * @brief Per-component storage policy trait selecting archetype table or sparse set.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once

namespace engine
{
    enum class StorageKind
    {
        Table,
        SparseSet,
    };

    // opt in: template<> struct ComponentStorageKind<Frozen> { static constexpr StorageKind VALUE = StorageKind::SparseSet; };
    template <typename T>
    struct ComponentStorageKind
    {
        static constexpr StorageKind VALUE = StorageKind::Table; // default
    };
}
