#include <engine/core/Run.hpp>

#include <engine/core/Application.hpp>
#include <engine/core/FrameTimer.hpp>
#include <engine/core/IFramePacer.hpp>
#include <engine/core/profile/Profile.hpp>

namespace engine
{
    int Run(Application &app)
    {
        app.OnInit();

        // post-init: OnShutdown must run even if loop throws. rethrow after.
        try
        {
            FrameTimer timer;
            while (app.IsRunning())
            {
                const float dt = timer.Tick();
                {
                    ENGINE_PROFILE_SCOPE("Update");
                    app.OnUpdate(dt);
                }
                {
                    ENGINE_PROFILE_SCOPE("Pace");
                    app.Pacer().EndFrame(timer.Stats());
                }
                ENGINE_PROFILE_FRAME();
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
