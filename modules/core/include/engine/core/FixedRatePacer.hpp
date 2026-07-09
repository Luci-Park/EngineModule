#pragma once
#include <engine/core/IFramePacer.hpp>

#include <cstdint>

namespace engine
{
    // Caps the loop to a target frame rate. Strategy: sleep for most of the
    // remaining budget, then busy-spin the last sliver for precision (OS sleep is
    // coarse). See .cpp for the Windows timer-resolution caveat.
    class ENGINE_CORE_API FixedRatePacer final : public IFramePacer
    {
    public:
        explicit FixedRatePacer(float targetFps);

        void EndFrame(const FrameStats& stats) override;

        void SetTargetFps(float fps);
        float TargetFps() const { return m_targetFps; }

    private:
        float m_targetFps;
        int64_t m_deadlineNs = 0;  // absolute steady-clock deadline for the current frame
        bool m_armed = false;      // false until the first EndFrame seeds the deadline
    };
}
