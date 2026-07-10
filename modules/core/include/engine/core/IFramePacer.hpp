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

        // end of frame. may block to cap rate.
        virtual void EndFrame(const FrameStats& stats) = 0;
    };
}
