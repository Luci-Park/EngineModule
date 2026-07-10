#pragma once
#include <engine/core/core_export.h>

namespace engine
{
    class IFramePacer;

    // application base, to be overriden and sent to engine::Run()
    // can control loop with running flag and pacer(for framerates)
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

        // framepacer = controls frames
        // default = unfixed
        IFramePacer &Pacer() const;
        void SetPacer(IFramePacer &pacer) { m_pacer = &pacer; }

    private:
        bool m_running = true;
        IFramePacer *m_pacer = nullptr;
    };
}
