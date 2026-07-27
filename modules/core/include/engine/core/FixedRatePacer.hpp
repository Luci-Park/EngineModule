/**
 * @file FixedRatePacer.hpp
 * @author sumin.park
 * @brief Frame pacer that caps the loop to a target frame rate.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include <engine/core/IFramePacer.hpp>

#include <cstdint>

namespace engine
{
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
