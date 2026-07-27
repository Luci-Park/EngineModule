/**
 * @file main.cpp
 * @author sumin.park
 * @brief Sandbox entry point.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#include "SandboxApp.hpp"
#include <engine/core/Run.hpp>

int main()
{
    SandboxApp app;
    return engine::Run(app);
}
