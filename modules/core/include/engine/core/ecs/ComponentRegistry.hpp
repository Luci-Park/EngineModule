/**
 * @file ComponentRegistry.hpp
 * @author sumin.park
 * @brief World-owned registry of ComponentInfo records, keyed by seq, with an init-time freeze.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include <engine/core/core_export.h>
#include <engine/core/ecs/ComponentInfo.hpp>
#include <engine/core/ecs/TypeId.hpp>
#include <engine/core/log/Assert.hpp>

#include <cstdint>
#include <unordered_map>

namespace engine
{
    class ENGINE_CORE_API ComponentRegistry
    {
    public:
        // idempotent: an already-known T returns its existing record untouched
        template <typename T>
        const ComponentInfo &Register()
        {
            const uint32_t seq = TypeIdOf<T>().m_seq;
            auto it = m_infos.find(seq);
            if (it != m_infos.end())
                return it->second;

            ENGINE_ASSERT(!m_frozen, "ComponentRegistry::Register: registry is frozen, T is not already known");
            return m_infos.emplace(seq, MakeComponentInfo<T>()).first->second;
        }

        const ComponentInfo *Find(uint32_t seq) const;
        const ComponentInfo &Get(uint32_t seq) const; // asserts if unknown

        template <typename T>
        const ComponentInfo *Find() const
        {
            return Find(TypeIdOf<T>().m_seq);
        }

        bool Contains(uint32_t seq) const;
        std::size_t Size() const;

        void Freeze();
        bool IsFrozen() const;

        void Validate() const;

    private:
        std::unordered_map<uint32_t, ComponentInfo> m_infos;
        bool m_frozen = false;
    };
}
