/**
 * @file Query.hpp
 * @author sumin.park
 * @brief Entity query: archetype-signature matching for table components, per-row probe for sparse ones.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include <engine/core/ecs/Column.hpp>
#include <engine/core/ecs/ComponentMeta.hpp>
#include <engine/core/ecs/Entity.hpp>
#include <engine/core/ecs/Mut.hpp>
#include <engine/core/ecs/QueryFilters.hpp>
#include <engine/core/ecs/SparseStorage.hpp>
#include <engine/core/ecs/StorageKind.hpp>
#include <engine/core/ecs/Table.hpp>
#include <engine/core/ecs/TypeId.hpp>
#include <engine/core/ecs/World.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <vector>

namespace engine
{
    namespace detail
    {
        template <typename P>
        struct IsEntityParam : std::false_type
        {
        };
        template <>
        struct IsEntityParam<Entity> : std::true_type
        {
        };

        template <typename P>
        struct IsWithoutParam : std::false_type
        {
        };
        template <typename T>
        struct IsWithoutParam<Without<T>> : std::true_type
        {
        };

        template <typename P>
        struct IsChangedParam : std::false_type
        {
        };
        template <typename T>
        struct IsChangedParam<Changed<T>> : std::true_type
        {
        };

        template <typename P>
        struct IsAddedParam : std::false_type
        {
        };
        template <typename T>
        struct IsAddedParam<Added<T>> : std::true_type
        {
        };

        template <typename P>
        struct ParamComponent
        {
            using Type = std::remove_const_t<P>;
        };
        template <typename T>
        struct ParamComponent<With<T>>
        {
            using Type = T;
        };
        template <typename T>
        struct ParamComponent<Without<T>>
        {
            using Type = T;
        };
        template <typename T>
        struct ParamComponent<Changed<T>>
        {
            using Type = T;
        };
        template <typename T>
        struct ParamComponent<Added<T>>
        {
            using Type = T;
        };
        template <typename P>
        using ParamComponentT = typename ParamComponent<P>::Type;

        template <typename P, bool = IsEntityParam<P>::value>
        struct ParamIsSparseImpl
        {
            static constexpr bool VALUE = ComponentStorageKind<ParamComponentT<P>>::VALUE == StorageKind::SparseSet;
        };
        template <typename P>
        struct ParamIsSparseImpl<P, true>
        {
            static constexpr bool VALUE = false;
        };
        template <typename P>
        inline constexpr bool PARAM_IS_SPARSE_V = ParamIsSparseImpl<P>::VALUE;

        // TDD sec.10 yield rule: Entity -> Entity | const C -> const C& | C -> Mut<C>
        template <typename P>
        struct FetchYield
        {
            using C = std::remove_const_t<P>;
            using Type = std::conditional_t<IsEntityParam<P>::value, Entity,
                                             std::conditional_t<std::is_const_v<P>, const C &, Mut<C>>>;
        };

        template <typename... Ts>
        struct TypeList
        {
        };

        template <typename Acc, typename... Ts>
        struct FilterFetchImpl;

        template <typename... Acc>
        struct FilterFetchImpl<TypeList<Acc...>>
        {
            using Type = TypeList<Acc...>;
        };

        template <typename... Acc, typename Head, typename... Tail>
        struct FilterFetchImpl<TypeList<Acc...>, Head, Tail...>
        {
            using Type = std::conditional_t<IS_FILTER_V<Head>,
                                             typename FilterFetchImpl<TypeList<Acc...>, Tail...>::Type,
                                             typename FilterFetchImpl<TypeList<Acc..., Head>, Tail...>::Type>;
        };

        template <typename... Ps>
        using FetchList = typename FilterFetchImpl<TypeList<>, Ps...>::Type;

        template <typename List>
        struct YieldTupleOf;
        template <typename... Fs>
        struct YieldTupleOf<TypeList<Fs...>>
        {
            using Type = std::tuple<typename FetchYield<Fs>::Type...>;
        };

        template <typename... Ps>
        using QueryTuple = typename YieldTupleOf<FetchList<Ps...>>::Type;

        template <typename List, typename U>
        struct AppendUnique;
        template <typename... Ts, typename U>
        struct AppendUnique<TypeList<Ts...>, U>
        {
            using Type = std::conditional_t<(std::is_same_v<Ts, U> || ...), TypeList<Ts...>, TypeList<Ts..., U>>;
        };

        template <typename Acc, typename... Ps>
        struct SparseTypesImpl;
        template <typename... Acc>
        struct SparseTypesImpl<TypeList<Acc...>>
        {
            using Type = TypeList<Acc...>;
        };
        template <typename... Acc, typename Head, typename... Tail>
        struct SparseTypesImpl<TypeList<Acc...>, Head, Tail...>
        {
            using Next = std::conditional_t<PARAM_IS_SPARSE_V<Head>,
                                             typename AppendUnique<TypeList<Acc...>, ParamComponentT<Head>>::Type,
                                             TypeList<Acc...>>;
            using Type = typename SparseTypesImpl<Next, Tail...>::Type;
        };

        template <typename List>
        struct SparsePtrTupleOf;
        template <typename... Cs>
        struct SparsePtrTupleOf<TypeList<Cs...>>
        {
            using Type = std::tuple<SparseStorage<Cs> *...>;
        };

        // deduplicated: two params over the same sparse C share one cached storage ptr
        template <typename... Ps>
        using SparsePtrTuple = typename SparsePtrTupleOf<typename SparseTypesImpl<TypeList<>, Ps...>::Type>::Type;
    }

    // Query<Ps...>: flat pack of fetch types (Entity / const T / T) and filter tags
    // (With / Without / Changed / Added). Table components match by cached archetype
    // signature, sparse components probe per row. See .claude/plans/06_query.md.
    template <typename... Ps>
    class Query
    {
        static_assert(sizeof...(Ps) > 0, "Query requires at least one param");

        using ValueType = detail::QueryTuple<Ps...>;
        static_assert(std::tuple_size_v<ValueType> != 0,
                      "Query requires at least one non-filter (fetch) param; an all-filter pack "
                      "would yield an empty tuple forever");

    public:
        // lastRunTick 0 (default) matches everything: live ticks start at 1, compare is '>'
        explicit Query(World &world, uint32_t lastRunTick = 0)
            : m_world(&world), m_lastRunTick(lastRunTick)
        {
            (CollectRequirement<Ps>(), ...);
            Refresh();
        }

        class Iterator
        {
        public:
            ValueType operator*() const
            {
                Table &table = m_query->ArchetypeAt(m_archetypeIdx).m_table;
                return m_query->MakeValue(table, m_row, table.EntityAt(m_row));
            }

            Iterator &operator++()
            {
                ++m_row;
                SkipToValid();
                return *this;
            }

            // m_query included: equal positions in two different Query objects are not equal iterators
            bool operator!=(const Iterator &other) const
            {
                return m_query != other.m_query || m_archetypeIdx != other.m_archetypeIdx || m_row != other.m_row;
            }

            static Iterator MakeBegin(Query *query)
            {
                Iterator it(query, 0, 0);
                it.SkipToValid();
                return it;
            }

            static Iterator MakeEnd(Query *query)
            {
                return Iterator(query, query->m_matched.size(), 0);
            }

        private:
            Iterator(Query *query, std::size_t archetypeIdx, std::size_t row)
                : m_query(query), m_archetypeIdx(archetypeIdx), m_row(row)
            {
            }

            void SkipToValid()
            {
                while (m_archetypeIdx < m_query->m_matched.size())
                {
                    Table &table = m_query->ArchetypeAt(m_archetypeIdx).m_table;
                    const std::size_t rowCount = table.RowCount();
                    while (m_row < rowCount)
                    {
                        if (m_query->RowMatches(table, m_row, table.EntityAt(m_row)))
                            return;
                        ++m_row;
                    }
                    ++m_archetypeIdx;
                    m_row = 0;
                }
                m_archetypeIdx = m_query->m_matched.size(); // canonical end == (matched.size(), 0)
                m_row = 0;
            }

            Query *m_query = nullptr;
            std::size_t m_archetypeIdx = 0;
            std::size_t m_row = 0;
        };

        // both refresh: an archetype or sparse storage created after construction must not be
        // silently dropped. end() refreshes too, so the pair is consistent whichever the caller
        // evaluates first.
        Iterator begin()
        {
            Refresh();
            return Iterator::MakeBegin(this);
        }

        Iterator end()
        {
            Refresh();
            return Iterator::MakeEnd(this);
        }

        // iterates; test/debug convenience, not a hot-path API
        std::size_t Count()
        {
            std::size_t n = 0;
            for (Iterator it = begin(), stop = end(); it != stop; ++it)
                ++n;
            return n;
        }

    private:
        World::Archetype &ArchetypeAt(std::size_t matchedIdx) const
        {
            return *m_world->m_archetypes[m_matched[matchedIdx]];
        }

        template <typename P>
        void CollectRequirement()
        {
            if constexpr (detail::IsEntityParam<P>::value)
            {
                // Entity imposes no matching constraint; deliberately empty
            }
            else
            {
                using C = detail::ParamComponentT<P>;
                if constexpr (detail::PARAM_IS_SPARSE_V<P>)
                {
                    // sparse never constrains archetype matching; probed per row
                }
                else
                {
                    const uint32_t seq = TypeIdOf<C>().m_seq;
                    if constexpr (detail::IsWithoutParam<P>::value)
                        m_excludedTableSeqs.push_back(seq);
                    else
                        m_requiredTableSeqs.push_back(seq); // fetch / With / Changed / Added all imply presence
                }
            }
        }

        void Refresh()
        {
            MatchArchetypes();
            RefreshSparse();
        }

        // m_archetypes is append-only (archetypes are never destroyed), so its size is itself the
        // generation counter: rescanning only the new tail keeps existing matches valid
        void MatchArchetypes()
        {
            const std::size_t archetypeCount = m_world->m_archetypes.size();
            for (std::size_t id = m_scannedArchetypeCount; id < archetypeCount; ++id)
            {
                const std::vector<uint32_t> &signature = m_world->m_archetypes[id]->m_signature;

                bool matches = true;
                for (uint32_t required : m_requiredTableSeqs)
                {
                    if (!std::binary_search(signature.begin(), signature.end(), required))
                    {
                        matches = false;
                        break;
                    }
                }
                if (matches)
                {
                    for (uint32_t excluded : m_excludedTableSeqs)
                    {
                        if (std::binary_search(signature.begin(), signature.end(), excluded))
                        {
                            matches = false;
                            break;
                        }
                    }
                }

                if (matches)
                    m_matched.push_back(static_cast<uint32_t>(id));
            }
            m_scannedArchetypeCount = archetypeCount;
        }

        void RefreshSparse()
        {
            std::apply([this](auto *&...slots) { (ResolveSparseSlot(slots), ...); }, m_sparse);
        }

        // a storage does not exist until the first AddComponent<C>, so a null slot is retried every
        // refresh. Once resolved the address is stable (unique_ptr in the map: a rehash moves the
        // pointer, not the object), so a resolved slot is never looked up again.
        template <typename C>
        void ResolveSparseSlot(SparseStorage<C> *&slot)
        {
            if (slot == nullptr)
                slot = static_cast<SparseStorage<C> *>(m_world->FindSparseStorage(TypeIdOf<C>().m_seq));
        }

        template <typename C>
        SparseStorage<C> *SparseFor() const
        {
            return std::get<SparseStorage<C> *>(m_sparse);
        }

        template <typename P>
        bool MetaPasses(const ComponentMeta *meta) const
        {
            if constexpr (detail::IsChangedParam<P>::value)
            {
                if (meta->m_changedTick <= m_lastRunTick)
                    return false;
            }
            if constexpr (detail::IsAddedParam<P>::value)
            {
                if (meta->m_addedTick <= m_lastRunTick)
                    return false;
            }
            return true;
        }

        template <typename P>
        bool ParamMatches(Table &table, std::size_t row, Entity e) const
        {
            if constexpr (detail::IsEntityParam<P>::value)
            {
                (void)table;
                (void)row;
                (void)e;
                return true;
            }
            else
            {
                using C = detail::ParamComponentT<P>;

                if constexpr (detail::IsWithoutParam<P>::value)
                {
                    if constexpr (detail::PARAM_IS_SPARSE_V<P>)
                    {
                        (void)table;
                        (void)row;
                        SparseStorage<C> *storage = SparseFor<C>();
                        return storage == nullptr || !storage->Contains(e);
                    }
                    else
                    {
                        (void)table;
                        (void)row;
                        (void)e;
                        return true; // table-kind Without already excluded at archetype-match level
                    }
                }
                else if constexpr (detail::PARAM_IS_SPARSE_V<P>)
                {
                    (void)table;
                    (void)row;
                    SparseStorage<C> *storage = SparseFor<C>();
                    if (storage == nullptr)
                        return false;

                    const typename SparseStorage<C>::Entry entry = storage->Find(e); // presence + meta, one lookup
                    if (entry.m_value == nullptr)
                        return false;
                    return MetaPasses<P>(entry.m_meta);
                }
                else if constexpr (detail::IsChangedParam<P>::value || detail::IsAddedParam<P>::value)
                {
                    (void)e;
                    return MetaPasses<P>(&table.GetColumn<C>()->Meta(row));
                }
                else
                {
                    (void)table;
                    (void)row;
                    (void)e;
                    return true; // table-kind presence already guaranteed by the archetype match
                }
            }
        }

        bool RowMatches(Table &table, std::size_t row, Entity e) const
        {
            bool ok = true;
            ((ok = ok && ParamMatches<Ps>(table, row, e)), ...);
            return ok;
        }

        template <typename F>
        typename detail::FetchYield<F>::Type FetchOne(Table &table, std::size_t row, Entity e) const
        {
            if constexpr (detail::IsEntityParam<F>::value)
            {
                (void)table;
                (void)row;
                return e;
            }
            else
            {
                using C = std::remove_const_t<F>;
                World &world = *m_world;

                if constexpr (detail::PARAM_IS_SPARSE_V<F>)
                {
                    (void)table;
                    (void)row;
                    const typename SparseStorage<C>::Entry entry = SparseFor<C>()->Find(e); // presence proven by RowMatches
                    if constexpr (std::is_const_v<F>)
                        return *entry.m_value;
                    else
                        return Mut<C>{entry.m_value, &entry.m_meta->m_changedTick, world.CurrentTick(),
                                      &world.m_structuralVersion, world.m_structuralVersion};
                }
                else
                {
                    (void)e;
                    Column<C> *column = table.GetColumn<C>();
                    if constexpr (std::is_const_v<F>)
                        return column->Get(row);
                    else
                        return Mut<C>{&column->Get(row), &column->Meta(row).m_changedTick, world.CurrentTick(),
                                      &world.m_structuralVersion, world.m_structuralVersion};
                }
            }
        }

        template <typename... Fs>
        ValueType MakeValueImpl(detail::TypeList<Fs...>, Table &table, std::size_t row, Entity e) const
        {
            return ValueType{FetchOne<Fs>(table, row, e)...};
        }

        ValueType MakeValue(Table &table, std::size_t row, Entity e) const
        {
            return MakeValueImpl(detail::FetchList<Ps...>{}, table, row, e);
        }

        World *m_world;
        uint32_t m_lastRunTick;
        std::vector<uint32_t> m_requiredTableSeqs;
        std::vector<uint32_t> m_excludedTableSeqs;
        std::vector<uint32_t> m_matched;
        std::size_t m_scannedArchetypeCount = 0;
        detail::SparsePtrTuple<Ps...> m_sparse{};
    };
}
