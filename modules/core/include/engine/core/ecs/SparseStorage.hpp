/**
 * @file SparseStorage.hpp
 * @author sumin.park
 * @brief World-global sparse-set storage for high-churn or rare components.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include <engine/core/ecs/ComponentMeta.hpp>
#include <engine/core/ecs/Entity.hpp>
#include <engine/core/log/Assert.hpp>

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

namespace engine
{
    // type-erased handle to one world-global sparse component store
    struct ISparseStorage
    {
        virtual ~ISparseStorage() = default;

        virtual bool Remove(Entity e) = 0; // false if absent; return gates the caller's structural bump
        virtual bool Contains(Entity e) const = 0;
        virtual std::size_t Size() const = 0;
        virtual Entity DenseEntityAt(std::size_t denseIndex) const = 0;
        virtual void Validate() const = 0;
        virtual void *DataFor(Entity e) = 0; // type-erased component bytes for hooks, nullptr if absent
    };

    template <typename T>
    class SparseStorage final : public ISparseStorage
    {
        // insert/remove relocate dense rows -> a throwing move desyncs the parallel arrays
        static_assert(std::is_trivially_copyable_v<T>,
                      "ECS component T must be trivially copyable");
        static_assert(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T>,
                      "ECS component T must be nothrow-movable");

    public:
        // overwrites an existing component, preserving its dense slot
        void Insert(Entity e, T value, ComponentMeta meta = {})
        {
            if (e.IsNull())
                return;

            EnsureSparseCapacity(e.m_index);

            const uint32_t dense = m_sparse[e.m_index];

            if (dense != INVALID)
            {
                m_dense[dense] = std::move(value);
                m_meta[dense] = meta;
                m_entities[dense] = e;
                return;
            }

            ReserveDenseOneMore();
            const uint32_t newDense = static_cast<uint32_t>(m_dense.size());
            m_dense.push_back(std::move(value));
            m_meta.push_back(meta);
            m_entities.push_back(e);
            m_sparse[e.m_index] = newDense;
        }

        bool Remove(Entity e) override
        {
            if (!Contains(e))
                return false;

            const uint32_t dense = m_sparse[e.m_index];
            const std::size_t last = m_dense.size() - 1;

            if (dense != last)
            {
                m_dense[dense] = std::move(m_dense[last]);
                m_meta[dense] = m_meta[last];
                m_entities[dense] = m_entities[last];
                m_sparse[m_entities[dense].m_index] = dense;
            }
            m_dense.pop_back();
            m_meta.pop_back();
            m_entities.pop_back();
            m_sparse[e.m_index] = INVALID;
            return true;
        }

        bool Contains(Entity e) const override
        {
            if (e.IsNull())
                return false;
            if (e.m_index >= m_sparse.size())
                return false;

            const uint32_t dense = m_sparse[e.m_index];
            return dense != INVALID && m_entities[dense] == e; // if generation not match, different entity
        }

        T *Get(Entity e)
        {
            if (!Contains(e))
                return nullptr;
            return &m_dense[m_sparse[e.m_index]];
        }

        const T *Get(Entity e) const
        {
            if (!Contains(e))
                return nullptr;
            return &m_dense[m_sparse[e.m_index]];
        }

        struct Entry
        {
            T *m_value;
            ComponentMeta *m_meta;
        };

        Entry Find(Entity e)
        {
            if (!Contains(e))
                return {nullptr, nullptr};
            const uint32_t dense = m_sparse[e.m_index];
            return {&m_dense[dense], &m_meta[dense]};
        }

        ComponentMeta *Meta(Entity e)
        {
            if (!Contains(e))
                return nullptr;
            return &m_meta[m_sparse[e.m_index]];
        }

        std::size_t Size() const override { return m_dense.size(); }

        Entity DenseEntityAt(std::size_t denseIndex) const override
        {
            ENGINE_ASSERT(denseIndex < m_entities.size(), "SparseStorage::DenseEntityAt: index out of range");
            return m_entities[denseIndex];
        }

        void *DataFor(Entity e) override { return Get(e); }

        void Validate() const override
        {
#ifndef NDEBUG
            ENGINE_ASSERT(m_dense.size() == m_meta.size() && m_dense.size() == m_entities.size(),
                          "SparseStorage::Validate: dense/meta/entities size desync");
            for (std::size_t dense = 0; dense < m_entities.size(); ++dense)
            {
                const Entity e = m_entities[dense];
                ENGINE_ASSERT(e.m_index < m_sparse.size(), "SparseStorage::Validate: dense entity has no sparse slot");
                ENGINE_ASSERT(m_sparse[e.m_index] == dense, "SparseStorage::Validate: sparse<->dense bijection broken");
            }
#endif
        }

    private:
        static constexpr uint32_t INVALID = UINT32_MAX;

        void EnsureSparseCapacity(uint32_t index)
        {
            if (index >= m_sparse.size())
            {
                m_sparse.resize(index + 1, INVALID);
            }
        }

        void ReserveDenseOneMore()
        {
            const std::size_t needed = m_dense.size() + 1;
            if (m_dense.capacity() < needed || m_meta.capacity() < needed || m_entities.capacity() < needed)
            {
                const std::size_t grown = m_dense.capacity() == 0 ? 1 : m_dense.capacity() * 2;
                const std::size_t target = grown < needed ? needed : grown;
                m_dense.reserve(target);
                m_meta.reserve(target);
                m_entities.reserve(target);
            }
        }

        std::vector<uint32_t> m_sparse;    // entity index -> dense index, or invalid
        std::vector<T> m_dense;            // packed
        std::vector<ComponentMeta> m_meta; // parallel to m_dense
        std::vector<Entity> m_entities;    // dense index -> entity (carries generation)
    };
}
