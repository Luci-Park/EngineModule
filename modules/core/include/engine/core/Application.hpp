#pragma once
#include <engine/core/core_export.h>

namespace engine
{
    class IFramePacer;

    // Abstract application base. Subclass it, override the lifecycle hooks, hand it
    // to engine::Run(). The base also holds loop-control state (running flag, pacer)
    // because the loop is app-driven: the app decides when to Close() and which
    // pacer is active.
    class ENGINE_CORE_API Application
    {
    public:
        virtual ~Application();

        virtual void OnInit() {}
        virtual void OnUpdate(float dt) = 0;
        virtual void OnShutdown() {}

        virtual void OnSuspend() {}
        virtual void OnResume() {}

        // Loop control (read by engine::Run).
        bool IsRunning() const { return m_running; }
        void Close() { m_running = false; }

        // Pacing. Defaults to an uncapped NullPacer until SetPacer() is called;
        // swappable at runtime (e.g. uncapped <-> fixed 30fps).
        // Non-owning: pacer must outlive this Application (or outlive the next
        // SetPacer()/Run() call, whichever is sooner). Never pass a temporary —
        // `SetPacer(FixedRatePacer(30.0f))` leaves a dangling pointer.
        IFramePacer& Pacer() const;
        void SetPacer(IFramePacer& pacer) { m_pacer = &pacer; }

    private:
        bool m_running = true;
        IFramePacer* m_pacer = nullptr;  // resolved to a shared default NullPacer in Pacer()
    };
}
