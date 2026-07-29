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
        // idempotent: an already-known T returns its existing record untouched.
        // returns nullptr only on a frozen absent-type violation; caller must check
        template <typename T>
        const ComponentInfo *Register()
        {
            const uint32_t seq = TypeIdOf<T>().m_seq;
            auto it = m_infos.find(seq);
            if (it != m_infos.end())
                return &it->second;

            if (m_frozen)
            {
                ENGINE_ASSERT(false, "ComponentRegistry::Register: registry is frozen, T is not already known");
                ENGINE_LOG_ERROR("ComponentRegistry::Register: rejecting new type registered after freeze");
                return nullptr;
            }
            return &m_infos.emplace(seq, MakeComponentInfo<T>()).first->second;
        }

        const ComponentInfo *Find(uint32_t seq) const;
        const ComponentInfo &Get(uint32_t seq) const; // asserts if unknown

        // the only mutating accessor on a record; hooks register during init like component
        // types do, so this shares the freeze flag with Register (see plan 08, decision C).
        // unconditional: unlike Register, an already-known T does not exempt this from the
        // frozen check; Register and SetHooks are independent ways to touch the same record,
        // so "T already has a record" says nothing about whether SetHooks itself ran before
        // init closed. delegates record creation to Register<T>() rather than reimplementing
        // find-or-insert here.
        // returns nullptr only on a frozen absent-type violation; caller must check
        template <typename T>
        const ComponentInfo *SetHooks(void (*onAdd)(World &, Entity, void *) noexcept, void (*onRemove)(World &, Entity, void *) noexcept)
        {
            if (m_frozen)
            {
                ENGINE_ASSERT(false, "ComponentRegistry::SetHooks: hooks must be attached before the registry is frozen");
                ENGINE_LOG_ERROR("ComponentRegistry::SetHooks: rejecting hooks attached after freeze");
                return nullptr;
            }
            Register<T>();
            ComponentInfo &info = m_infos.find(TypeIdOf<T>().m_seq)->second;
            info.m_onAdd = onAdd;
            info.m_onRemove = onRemove;
            return &info;
        }

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
