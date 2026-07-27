/**
 * @file Application.cpp
 * @author sumin.park
 * @brief Application lifetime and the lazily-created default frame pacer.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#include <engine/core/Application.hpp>
#include <engine/core/NullPacer.hpp>

namespace engine
{
    Application::~Application() = default;

    IFramePacer &Application::Pacer() const
    {
        static NullPacer s_default;
        return m_pacer ? *m_pacer : s_default;
    }
}
