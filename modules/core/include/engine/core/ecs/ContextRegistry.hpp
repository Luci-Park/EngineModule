/**
 * @file ContextRegistry.hpp
 * @author sumin.park
 * @brief World-owned registry of type-keyed singleton contexts, torn down in reverse install order.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include <engine/core/core_export.h>
#include <engine/core/ecs/Context.hpp>
#include <engine/core/ecs/TypeId.hpp>
#include <engine/core/log/Assert.hpp>

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace engine
{
    class ENGINE_CORE_API ContextRegistry
    {
    public:
        ContextRegistry() = default;

        // holds unique_ptr (move-only) and is dllexport'd: forced instantiation of the
        // implicit copy ops would fail (C2280) unless spelled out
        // move ops are deleted, not defaulted (unit 10, chunk 6): a defaulted move-assign
        // drops the destination's contexts without reverse-order TearDown()
        ContextRegistry(const ContextRegistry &) = delete;
        ContextRegistry &operator=(const ContextRegistry &) = delete;
        ContextRegistry(ContextRegistry &&) = delete;
        ContextRegistry &operator=(ContextRegistry &&) = delete;
        ~ContextRegistry(); // TearDown(): reverse install order

        // present -> assert. "I provide this; a clash is a bug". see InitContext/OverrideContext
        // for the other two install verbs (decision B); each declares intent, none infers it
        // returns nullptr on collision (already installed) or on a frozen absent-type
        // violation (see InstallNew); callers must check before deref
        template <typename T>
        T *Set(T value)
        {
            const uint32_t seq = TypeIdOf<T>().m_seq;
            auto it = m_contexts.find(seq);
            if (it != m_contexts.end())
            {
                ENGINE_ASSERT(false, "ContextRegistry::Set: T is already installed; use InitContext or OverrideContext");
                ENGINE_LOG_ERROR("ContextRegistry::Set: T already installed; rejecting new value");
                return nullptr;
            }
            return InstallNew(seq, std::move(value));
        }

        // present -> no-op, returns the EXISTING value; the passed-in one is discarded.
        // "provide a default unless someone already did", per DefaultPlugins
        // returns nullptr only on a frozen absent-type violation (see InstallNew)
        template <typename T>
        T *Init(T value)
        {
            const uint32_t seq = TypeIdOf<T>().m_seq;
            auto it = m_contexts.find(seq);
            if (it != m_contexts.end())
                return &static_cast<ContextHolder<T> &>(*it->second).m_value;
            return InstallNew(seq, std::move(value));
        }

        // present -> replace, running the old value's dtor; KEEPS the type's install-order
        // position (decision B: "override" is the same slot with a different value, and moving
        // it would retroactively invalidate dependencies established at first install)
        // returns nullptr only on a frozen absent-type violation (see InstallNew)
        template <typename T>
        T *Override(T value)
        {
            const uint32_t seq = TypeIdOf<T>().m_seq;
            auto it = m_contexts.find(seq);
            if (it == m_contexts.end())
                return InstallNew(seq, std::move(value));

            // construct the new holder before dropping the old one, so a throwing T
            // constructor leaves the previous value intact rather than an empty slot.
            // no freeze check here: overriding a present type is legal forever, since the
            // freeze gates the type set, not values (decision C)
            it->second = std::make_unique<ContextHolder<T>>(std::move(value));
            return &static_cast<ContextHolder<T> &>(*it->second).m_value;
        }

        template <typename T>
        T &Get()
        {
            auto it = m_contexts.find(TypeIdOf<T>().m_seq);
            ENGINE_ASSERT(it != m_contexts.end(), "ContextRegistry::Get: T is not installed");
            return static_cast<ContextHolder<T> &>(*it->second).m_value;
        }

        template <typename T>
        const T &Get() const
        {
            auto it = m_contexts.find(TypeIdOf<T>().m_seq);
            ENGINE_ASSERT(it != m_contexts.end(), "ContextRegistry::Get: T is not installed");
            return static_cast<const ContextHolder<T> &>(*it->second).m_value;
        }

        template <typename T>
        T *TryGet()
        {
            auto it = m_contexts.find(TypeIdOf<T>().m_seq);
            return it == m_contexts.end() ? nullptr : &static_cast<ContextHolder<T> &>(*it->second).m_value;
        }

        template <typename T>
        const T *TryGet() const
        {
            auto it = m_contexts.find(TypeIdOf<T>().m_seq);
            return it == m_contexts.end() ? nullptr : &static_cast<const ContextHolder<T> &>(*it->second).m_value;
        }

        template <typename T>
        bool Has() const
        {
            return m_contexts.find(TypeIdOf<T>().m_seq) != m_contexts.end();
        }

        std::size_t Size() const;

        // gates the type set only, not values: Set/Init/Override all assert on an ABSENT type
        // once frozen; Init on a present type stays a no-op and Override on a present type
        // stays legal forever (decision C): section 13 freezes the type set, not the values
        void Freeze();
        bool IsFrozen() const;

        // reverse install order: last-installed first. explicit rather than left implicit in
        // ~ContextRegistry so World can order it against the hook sweep in ~World; see
        // plan 09's teardown rules. idempotent: safe to call more than once.
        void TearDown();

        void Validate() const;

    private:
        // shared absent-path insert for Set/Init/Override (decision C: freeze gates only this
        // path). single frozen-check message rather than one per verb, since the three call
        // sites already say which verb asserted via ENGINE_ASSERT's own file/line output
        template <typename T>
        T *InstallNew(uint32_t seq, T value)
        {
            if (m_frozen)
            {
                ENGINE_ASSERT(false, "ContextRegistry: cannot install a new context type after Freeze()");
                ENGINE_LOG_ERROR("ContextRegistry: rejecting new context type installed after Freeze()");
                return nullptr;
            }
            auto it = m_contexts.emplace(seq, std::make_unique<ContextHolder<T>>(std::move(value))).first;
            m_installOrder.push_back(seq);
            return &static_cast<ContextHolder<T> &>(*it->second).m_value;
        }

        std::unordered_map<uint32_t, std::unique_ptr<IContext>> m_contexts;
        std::vector<uint32_t> m_installOrder; // teardown walks this backwards
        bool m_frozen = false;
    };
}
