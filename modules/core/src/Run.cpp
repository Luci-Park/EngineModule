#include <engine/core/Run.hpp>

#include <engine/core/Application.hpp>
#include <engine/core/FrameTimer.hpp>
#include <engine/core/IFramePacer.hpp>

namespace engine
{
    int Run(Application& app)
    {
        // OnShutdown must run even if init/update throws, so cleanup (files, GPU/audio
        // handles) always happens. Rethrow after shutdown so the caller still sees the error.
        try
        {
            app.OnInit();

            FrameTimer timer;
            while (app.IsRunning())
            {
                const float dt = timer.Tick();
                app.OnUpdate(dt);
                app.Pacer().EndFrame(timer.Stats());
            }
        }
        catch (...)
        {
            app.OnShutdown();
            throw;
        }

        app.OnShutdown();
        return 0;
    }
}
