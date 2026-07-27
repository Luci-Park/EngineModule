/**
 * @file Run.hpp
 * @author sumin.park
 * @brief Entry point for the engine-owned frame loop.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include <engine/core/core_export.h>

namespace engine
{
    class Application;

    // engine loop lives here
    ENGINE_CORE_API int Run(Application &app);
}
