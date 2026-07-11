#pragma once
#include <engine/core/ecs/Column.hpp>
#include <engine/core/ecs/Entity.hpp>
#include <engine/core/ecs/TypeId.hpp>
#include <engine/core/log/Assert.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace engine
{
    // columns keyed by component seq, sharing one row space
    class Table
    {
    public:
        template <typename T>
        void AddColumn()
        {
            const uint32_t seq = TypeIdOf<T>().m_seq;
            ENGINE_ASSERT(m_columns.find(seq) == m_columns.end(), "Table::AddColumn: column already present for this component");
            m_columns.emplace(seq, std::make_unique<Column<T>>());
        }

        template <typename T>
        Column<T> *GetColumn()
        {
            const uint32_t seq = TypeIdOf<T>().m_seq;
            auto it = m_columns.find(seq);
            if (it == m_columns.end())
                return nullptr;
            return static_cast<Column<T> *>(it->second.get());
        }

        bool HasColumn(uint32_t seq) const { return m_columns.find(seq) != m_columns.end(); }

        std::size_t RowCount() const { return m_entities.size(); }

        Entity EntityAt(std::size_t row) const { return m_entities[row]; }

        std::size_t AddEntity(Entity e)
        {
            m_entities.push_back(e);
            return m_entities.size() - 1;
        }

        // destroy entity and all columns, fill it by moving in last entity
        Entity SwapRemove(std::size_t row)
        {
            ENGINE_ASSERT(row < m_entities.size(), "Table::SwapRemove: row out of range");

            const std::size_t last = m_entities.size() - 1;
            const bool moved = row != last;
            const Entity result = moved ? m_entities[last] : NULL_ENTITY;

            for (auto &[seq, column] : m_columns)
            {
                ENGINE_ASSERT(column->Size() == m_entities.size(), "Table::SwapRemove: column/row-count desync");
                column->SwapRemove(row);
            }

            if (moved)
            {
                m_entities[row] = m_entities[last];
            }
            m_entities.pop_back();

            return result;
        }

        // debug: every column Size()==RowCount(); no duplicate live entity
        void Validate() const
        {
#ifndef NDEBUG
            for (const auto &[seq, column] : m_columns)
            {
                ENGINE_ASSERT(column->Size() == RowCount(), "Table::Validate: column/row-count desync");
            }
            for (std::size_t i = 0; i < m_entities.size(); ++i)
            {
                for (std::size_t j = i + 1; j < m_entities.size(); ++j)
                {
                    ENGINE_ASSERT(!(m_entities[i] == m_entities[j]), "Table::Validate: duplicate live entity");
                }
            }
#endif
        }

    private:
        std::unordered_map<uint32_t, std::unique_ptr<IColumn>> m_columns;
        std::vector<Entity> m_entities;
    };
}
