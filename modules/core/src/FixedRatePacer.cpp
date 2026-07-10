#include <engine/core/FixedRatePacer.hpp>
#include <engine/core/FrameTimer.hpp>
#include "Clock.hpp"

#include <chrono>
#include <thread>

namespace engine
{
    namespace
    {
        // sleep to margin, spin the rest
        constexpr int64_t SPIN_MARGIN_NS = 2'000'000; // 2ms
    }

    FixedRatePacer::FixedRatePacer(float targetFps)
        : m_targetFps(targetFps > 0.0f ? targetFps : 60.0f)
    {
    }

    void FixedRatePacer::SetTargetFps(float fps)
    {
        m_targetFps = (fps > 0.0f) ? fps : m_targetFps;
    }

    void FixedRatePacer::EndFrame(const FrameStats & /*stats*/)
    {
        // target time per frame
        const int64_t budgetNs = static_cast<int64_t>(1'000'000'000.0 / static_cast<double>(m_targetFps));
        const int64_t now = NowNs();

        // first frame, just advance
        if (!m_armed)
        {
            m_deadlineNs = now + budgetNs;
            m_armed = true;
            return;
        }

        // the frame should end at
        m_deadlineNs += budgetNs;

        // if pace fell behind, resync instead of spinning
        if (m_deadlineNs < now)
        {
            m_deadlineNs = now + budgetNs;
            return;
        }

        const int64_t sleepUntil = m_deadlineNs - SPIN_MARGIN_NS;
        if (sleepUntil > now)
        {
            std::this_thread::sleep_for(std::chrono::nanoseconds(sleepUntil - now));
        }

        while (NowNs() < m_deadlineNs)
        {
            std::this_thread::yield();
        }
    }
}
