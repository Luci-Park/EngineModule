#pragma once
#include <engine/core/IFramePacer.hpp>

#include <cstdint>

namespace engine
{
    // caps to target fps
    class ENGINE_CORE_API FixedRatePacer final : public IFramePacer
    {
    public:
        explicit FixedRatePacer(float targetFps);

        void EndFrame(const FrameStats &stats) override;

        // applies next frame
        void SetTargetFps(float fps);
        float TargetFps() const { return m_targetFps; }

    private:
        float m_targetFps;
        int64_t m_deadlineNs = 0; // steady-clock deadline for current frame
        bool m_armed = false;     // false until first EndFrame seeds deadline
    };
}
