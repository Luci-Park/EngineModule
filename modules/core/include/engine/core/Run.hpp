#pragma once
#include <engine/core/core_export.h>

namespace engine
{
    class Application;

    // engine loop lives here
    ENGINE_CORE_API int Run(Application &app);
}
