#pragma once
#include <engine/core/ecs/ComponentMeta.hpp>
#include <engine/core/ecs/TypeId.hpp>
#include <engine/core/log/Assert.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace engine
{
    // component storage for architype
    // interface for type-erasure
    struct IColumn
    {
        virtual ~IColumn() = default;

        virtual void SwapRemove(std::size_t row) = 0;              // destroy row, fill it by moving in last row
        virtual void MoveRowTo(std::size_t row, IColumn &dst) = 0; // append row to dst, then SwapRemove(row) here
        virtual void Reserve(std::size_t additional) = 0;
        virtual std::size_t Size() const = 0;
        virtual uint32_t ComponentSeq() const = 0;
        virtual std::unique_ptr<IColumn> CloneEmpty() const = 0;
    };

    template <typename T>
    class Column final : public IColumn
    {
        // rows get relocated by SwapRemove/MoveRowTo -> a throwing move would desync the parallel
        static_assert(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T>,
                      "ECS component T must be nothrow-movable");

    public:
        void Push(T value, ComponentMeta meta = {})
        {
            ReserveOneMore();
            m_data.push_back(std::move(value));
            m_meta.push_back(meta);
        }

        T &Get(std::size_t row)
        {
            ENGINE_ASSERT(row < m_data.size(), "Column::Get: row out of range");
            return m_data[row];
        }

        const T &Get(std::size_t row) const
        {
            ENGINE_ASSERT(row < m_data.size(), "Column::Get: row out of range");
            return m_data[row];
        }

        ComponentMeta &Meta(std::size_t row)
        {
            ENGINE_ASSERT(row < m_meta.size(), "Column::Meta: row out of range");
            return m_meta[row];
        }

        void SwapRemove(std::size_t row) override
        {
            ENGINE_ASSERT(row < m_data.size(), "Column::SwapRemove: row out of range");

            const std::size_t last = m_data.size() - 1;
            if (row != last)
            {
                m_data[row] = std::move(m_data[last]);
                m_meta[row] = m_meta[last];
            }
            m_data.pop_back();
            m_meta.pop_back();
        }

        void MoveRowTo(std::size_t row, IColumn &dst) override
        {
            ENGINE_ASSERT(dst.ComponentSeq() == ComponentSeq(),
                          "Column::MoveRowTo: destination column holds a different component type");
            ENGINE_ASSERT(row < m_data.size(), "Column::MoveRowTo: row out of range");

            auto &typedDst = static_cast<Column<T> &>(dst);

            // growing dst should happen before changing source
            // change will not happen on exception at ReserveOneMore()
            typedDst.ReserveOneMore();
            typedDst.m_data.push_back(std::move(m_data[row]));
            typedDst.m_meta.push_back(m_meta[row]);
            SwapRemove(row);
        }

        void Reserve(std::size_t additional) override
        {
            const std::size_t needed = m_data.size() + additional;
            if (m_data.capacity() < needed || m_meta.capacity() < needed)
            {
                const std::size_t grown = m_data.capacity() == 0 ? needed : m_data.capacity() * 2;
                const std::size_t target = grown < needed ? needed : grown;
                m_data.reserve(target);
                m_meta.reserve(target);
            }
        }

        std::size_t Size() const override { return m_data.size(); }

        uint32_t ComponentSeq() const override { return TypeIdOf<T>().m_seq; }

        std::unique_ptr<IColumn> CloneEmpty() const override { return std::make_unique<Column<T>>(); }

    private:
        // reserve room for both arrays
        void ReserveOneMore()
        {
            const std::size_t needed = m_data.size() + 1;
            if (m_data.capacity() < needed || m_meta.capacity() < needed)
            {
                const std::size_t grown = m_data.capacity() == 0 ? 1 : m_data.capacity() * 2;
                const std::size_t target = grown < needed ? needed : grown;
                m_data.reserve(target);
                m_meta.reserve(target);
            }
        }

        std::vector<T> m_data;
        std::vector<ComponentMeta> m_meta; // m_meta[i] belongs to m_data[i]
    };
}
