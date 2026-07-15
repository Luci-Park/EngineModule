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
        // basic
        template <typename T>
        void AddColumn()
        {
            AddColumn(TypeIdOf<T>().m_seq, std::make_unique<Column<T>>());
        }

        // used when column already exists elsewhere
        void AddColumn(uint32_t seq, std::unique_ptr<IColumn> column)
        {
            ENGINE_ASSERT(m_columns.find(seq) == m_columns.end(), "Table::AddColumn: column already present for this component");
            ENGINE_ASSERT(column->ComponentSeq() == seq, "Table::AddColumn: column type does not match seq key -- GetColumn<T> would static_cast to the wrong type");
            m_columns.emplace(seq, std::move(column));
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

        // used only when caller doesn't know each column's T
        IColumn *FindColumn(uint32_t seq)
        {
            auto it = m_columns.find(seq);
            return it == m_columns.end() ? nullptr : it->second.get();
        }

        const std::unordered_map<uint32_t, std::unique_ptr<IColumn>> &Columns() const { return m_columns; }

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

        // return the entity moved into srcRow
        Entity MoveRowTo(std::size_t srcRow, Table &dst)
        {
            ENGINE_ASSERT(srcRow < m_entities.size(), "Table::MoveRowTo: row out of range");

            const std::size_t last = m_entities.size() - 1;
            const bool moved = srcRow != last;
            const Entity displaced = moved ? m_entities[last] : NULL_ENTITY;

            const std::size_t neededEntities = dst.m_entities.size() + 1;
            if (dst.m_entities.capacity() < neededEntities)
            {
                const std::size_t grown = dst.m_entities.capacity() == 0 ? neededEntities : dst.m_entities.capacity() * 2;
                dst.m_entities.reserve(grown < neededEntities ? neededEntities : grown);
            }
            for (auto &[seq, column] : m_columns)
            {
                if (IColumn *dstColumn = dst.FindColumn(seq))
                    dstColumn->Reserve(1);
            }

            dst.m_entities.push_back(m_entities[srcRow]);
            for (auto &[seq, column] : m_columns)
            {
                if (IColumn *dstColumn = dst.FindColumn(seq))
                    column->MoveRowTo(srcRow, *dstColumn);
                else
                    column->SwapRemove(srcRow);
            }

            if (moved)
                m_entities[srcRow] = m_entities[last];
            m_entities.pop_back();

            return displaced;
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
