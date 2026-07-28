/**
 * @file ContextRegistry.cpp
 * @author sumin.park
 * @brief ContextRegistry non-template members: size and debug validation.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#include <engine/core/ecs/ContextRegistry.hpp>

namespace engine
{
    ContextRegistry::~ContextRegistry()
    {
        TearDown();
    }

    std::size_t ContextRegistry::Size() const
    {
        return m_contexts.size();
    }

    void ContextRegistry::Freeze()
    {
        m_frozen = true;
    }

    bool ContextRegistry::IsFrozen() const
    {
        return m_frozen;
    }

    void ContextRegistry::TearDown()
    {
        for (auto it = m_installOrder.rbegin(); it != m_installOrder.rend(); ++it)
            m_contexts.erase(*it);
        m_installOrder.clear();
    }

    void ContextRegistry::Validate() const
    {
#ifndef NDEBUG
        ENGINE_ASSERT(m_contexts.size() == m_installOrder.size(),
                      "ContextRegistry::Validate: map and install-order vector sizes disagree");

        for (std::size_t i = 0; i < m_installOrder.size(); ++i)
        {
            ENGINE_ASSERT(m_contexts.find(m_installOrder[i]) != m_contexts.end(),
                          "ContextRegistry::Validate: install-order entry has no matching context");
            for (std::size_t j = i + 1; j < m_installOrder.size(); ++j)
                ENGINE_ASSERT(m_installOrder[i] != m_installOrder[j],
                              "ContextRegistry::Validate: duplicate seq in install order");
        }
#endif
    }
}
