#pragma once
#include <engine/core/core_export.h>

namespace engine
{
    struct FrameStats;

    // Frame pacing strategy. Called once at the end of every frame. Implementations
    // decide whether/how long to stall so the loop hits a target cadence.
    class ENGINE_CORE_API IFramePacer
    {
    public:
        virtual ~IFramePacer() = default;

        // Called at end of frame with the frame's stats. May block to cap frame rate.
        virtual void EndFrame(const FrameStats& stats) = 0;
    };
}
