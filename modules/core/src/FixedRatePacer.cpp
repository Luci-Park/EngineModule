#include <engine/core/FixedRatePacer.hpp>
#include <engine/core/FrameTimer.hpp>
#include "Clock.hpp"

#include <chrono>
#include <thread>

namespace engine
{
    namespace
    {
        // Sleep this close to the deadline, then spin the rest. OS sleep granularity
        // is coarse (~1-15ms on Windows without timeBeginPeriod), so leave a margin
        // and busy-wait it out for accuracy.
        //
        // Windows timer-resolution note: default scheduler tick makes sleep_for round
        // up to ~15ms, which would make sub-15ms budgets miss. A production build can
        // raise resolution with timeBeginPeriod(1) (winmm) for the app's lifetime; we
        // deliberately avoid that dependency here and rely on the spin margin instead.
        constexpr int64_t SPIN_MARGIN_NS = 2'000'000;  // 2ms
    }

    FixedRatePacer::FixedRatePacer(float targetFps)
        : m_targetFps(targetFps > 0.0f ? targetFps : 60.0f)
    {
    }

    void FixedRatePacer::SetTargetFps(float fps)
    {
        m_targetFps = (fps > 0.0f) ? fps : m_targetFps;
        // Deliberately does NOT touch m_armed/m_deadlineNs: EndFrame recomputes budgetNs
        // from m_targetFps every call and applies it to the existing deadline, so a
        // retarget takes effect starting next frame without skipping a frame of pacing.
    }

    void FixedRatePacer::EndFrame(const FrameStats& /*stats*/)
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

        // If we fell far behind (e.g. a stall > one budget), don't try to "catch up"
        // by spinning through a backlog — resync to now so pacing stays smooth.
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
