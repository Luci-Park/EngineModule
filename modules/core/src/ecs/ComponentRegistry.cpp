/**
 * @file ComponentRegistry.cpp
 * @author sumin.park
 * @brief ComponentRegistry non-template members: lookup, freeze and debug validation.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#include <engine/core/ecs/ComponentRegistry.hpp>

namespace engine
{
    const ComponentInfo *ComponentRegistry::Find(uint32_t seq) const
    {
        auto it = m_infos.find(seq);
        return it == m_infos.end() ? nullptr : &it->second;
    }

    const ComponentInfo &ComponentRegistry::Get(uint32_t seq) const
    {
        auto it = m_infos.find(seq);
        ENGINE_ASSERT(it != m_infos.end(), "ComponentRegistry::Get: unknown component seq {}", seq);
        return it->second;
    }

    bool ComponentRegistry::Contains(uint32_t seq) const
    {
        return m_infos.find(seq) != m_infos.end();
    }

    std::size_t ComponentRegistry::Size() const
    {
        return m_infos.size();
    }

    void ComponentRegistry::Freeze()
    {
        m_frozen = true;
    }

    bool ComponentRegistry::IsFrozen() const
    {
        return m_frozen;
    }

    void ComponentRegistry::Validate() const
    {
#ifndef NDEBUG
        for (const auto &[seq, info] : m_infos)
        {
            ENGINE_ASSERT(info.m_seq == seq, "ComponentRegistry::Validate: seq key does not match record");
            ENGINE_ASSERT(info.m_makeColumn != nullptr && info.m_makeSparseStorage != nullptr,
                          "ComponentRegistry::Validate: factories must be non-null");
        }
#endif
    }
}
