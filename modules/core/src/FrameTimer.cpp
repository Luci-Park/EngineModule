#include <engine/core/FrameTimer.hpp>
#include "Clock.hpp"

namespace engine
{
    FrameTimer::FrameTimer()
        : m_lastNs(NowNs())
    {
    }

    float FrameTimer::Tick()
    {
        const int64_t now = NowNs();

        if (!m_started)
        {
            m_started = true;
            m_lastNs = now;
            return 0.0f;
        }

        const float dt = static_cast<float>(static_cast<double>(now - m_lastNs) * 1e-9);
        m_lastNs = now;

        Push(dt);

        m_stats.dt = dt;
        m_stats.fps = (dt > 0.0f) ? (1.0f / dt) : 0.0f;
        const double avgDt = (m_count > 0) ? (m_sum / static_cast<double>(m_count)) : 0.0;
        m_stats.avgFps = (avgDt > 0.0) ? static_cast<float>(1.0 / avgDt) : 0.0f;
        m_stats.minDt = m_min;
        m_stats.maxDt = m_max;

        return dt;
    }

    void FrameTimer::Push(float dt)
    {
        const bool wasFull = (m_count == HISTORY_CAPACITY);
        const float evicted = wasFull ? m_history[m_head] : 0.0f;
        const float oldMin = m_min;
        const float oldMax = m_max;

        m_history[m_head] = dt;
        m_head = (m_head + 1) % HISTORY_CAPACITY;
        if (!wasFull)
        {
            ++m_count;
        }

        m_sum += static_cast<double>(dt);
        if (wasFull)
        {
            m_sum -= static_cast<double>(evicted);
        }

        if (m_count == 1)
        {
            m_min = dt;
            m_max = dt;
            return;
        }

        if (wasFull && (evicted == oldMin || evicted == oldMax))
        {
            RescanMinMax();
        }
        else
        {
            if (dt < m_min)
                m_min = dt;
            if (dt > m_max)
                m_max = dt;
        }
    }

    void FrameTimer::RescanMinMax()
    {
        const uint32_t n = HistoryCount();
        const uint32_t start = (m_count < HISTORY_CAPACITY) ? 0u : m_head;

        float minDt = m_history[start];
        float maxDt = minDt;
        for (uint32_t i = 1; i < n; ++i)
        {
            const float v = m_history[(start + i) % HISTORY_CAPACITY];
            if (v < minDt)
                minDt = v;
            if (v > maxDt)
                maxDt = v;
        }
        m_min = minDt;
        m_max = maxDt;
    }
}
