/**
 * @file IFramePacer.hpp
 * @author sumin.park
 * @brief Frame pacing strategy interface, invoked at the end of every frame.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include <engine/core/core_export.h>

namespace engine
{
    struct FrameStats;

    // pacing strategy. called end of each frame -> may stall to hit a cadence.
    class ENGINE_CORE_API IFramePacer
    {
    public:
        virtual ~IFramePacer() = default;

        virtual void EndFrame(const FrameStats& stats) = 0;
    };
}
