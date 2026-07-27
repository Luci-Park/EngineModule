/**
 * @file Application.hpp
 * @author sumin.park
 * @brief Application base class the game overrides and hands to engine::Run.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include <engine/core/core_export.h>

namespace engine
{
    class IFramePacer;

    // application base; override and hand to engine::Run()
    class ENGINE_CORE_API Application
    {
    public:
        virtual ~Application();

        virtual void OnInit() {}
        virtual void OnUpdate(float dt) = 0;
        virtual void OnShutdown() {}

        virtual void OnSuspend() {}
        virtual void OnResume() {}

        bool IsRunning() const { return m_running; }
        void Close() { m_running = false; }

        // lazy shared default -> NullPacer, so this never returns null
        IFramePacer &Pacer() const;
        void SetPacer(IFramePacer &pacer) { m_pacer = &pacer; }

    private:
        bool m_running = true;
        IFramePacer *m_pacer = nullptr;
    };
}
