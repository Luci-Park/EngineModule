/**
 * @file ComponentInfo.hpp
 * @author sumin.park
 * @brief Per-component runtime record: identity, size, storage policy and type-erased factories.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include <engine/core/ecs/Column.hpp>
#include <engine/core/ecs/Entity.hpp>
#include <engine/core/ecs/SparseStorage.hpp>
#include <engine/core/ecs/StorageKind.hpp>
#include <engine/core/ecs/TypeId.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace engine
{
    class World;

    struct ComponentInfo
    {
        uint32_t m_seq = 0;
        uint32_t m_hash = 0;
        std::string_view m_name;

        std::size_t m_size = 0;
        std::size_t m_align = 0;

        StorageKind m_storage = StorageKind::Table;

        // plain function pointers: no capture, no heap, record stays trivially copyable
        std::unique_ptr<IColumn> (*m_makeColumn)() = nullptr;
        std::unique_ptr<ISparseStorage> (*m_makeSparseStorage)() = nullptr;

        // unit 08: no setter, no firing, no null-check branch anywhere in unit 07
        void (*m_onAdd)(World &, Entity, void *) = nullptr;
        void (*m_onRemove)(World &, Entity, void *) = nullptr;
    };

    // the only population path in this unit; the runtime field-schema path (script
    // components) is deferred with scripting
    template <typename T>
    ComponentInfo MakeComponentInfo()
    {
        const TypeId id = TypeIdOf<T>();

        ComponentInfo info;
        info.m_seq = id.m_seq;
        info.m_hash = id.m_hash;
        info.m_name = id.m_name;
        info.m_size = sizeof(T);
        info.m_align = alignof(T);
        info.m_storage = ComponentStorageKind<T>::VALUE;
        info.m_makeColumn = []() -> std::unique_ptr<IColumn>
        { return std::make_unique<Column<T>>(); };
        info.m_makeSparseStorage = []() -> std::unique_ptr<ISparseStorage>
        { return std::make_unique<SparseStorage<T>>(); };
        return info;
    }
}
