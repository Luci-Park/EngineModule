/**
 * @file FixedRatePacer.cpp
 * @author sumin.park
 * @brief Fixed-rate pacing: sleep to a margin, then spin to the frame deadline.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#include <engine/core/FixedRatePacer.hpp>
#include <engine/core/FrameTimer.hpp>
#include "Clock.hpp"

#include <chrono>
#include <thread>

namespace engine
{
    namespace
    {
        // sleep stops this far short of the deadline; the rest is spun
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
        const int64_t budgetNs = static_cast<int64_t>(1'000'000'000.0 / static_cast<double>(m_targetFps));
        const int64_t now = NowNs();

        if (!m_armed)
        {
            m_deadlineNs = now + budgetNs;
            m_armed = true;
            return;
        }

        m_deadlineNs += budgetNs;

        // behind schedule -> resync rather than burn the whole frame catching up
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
