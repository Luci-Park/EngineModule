#pragma once
#include <engine/core/core_export.h>

namespace engine
{
    class Application;

    // Owns the engine loop: init -> per-frame (dt, update, pace) -> shutdown.
    // Runs until the app requests Close(). Returns a process exit code.
    //
    // The loop is library code by rule — main() never contains it. Structured so a
    // fixed-update step can be added here later without touching Application.
    ENGINE_CORE_API int Run(Application& app);
}
