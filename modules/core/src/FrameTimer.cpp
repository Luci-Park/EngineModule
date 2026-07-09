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
        float dt = 0.0f;
        if (m_started)
        {
            dt = static_cast<float>(static_cast<double>(now - m_lastNs) * 1e-9);
        }
        m_lastNs = now;
        m_started = true;

        Push(dt);

        m_stats.dt = dt;
        m_stats.fps = (dt > 0.0f) ? (1.0f / dt) : 0.0f;
        const float avgDt = (m_count > 0) ? (m_sum / static_cast<float>(m_count)) : 0.0f;
        m_stats.avgFps = (avgDt > 0.0f) ? (1.0f / avgDt) : 0.0f;
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

        m_sum += dt;
        if (wasFull)
        {
            m_sum -= evicted;
        }

        if (m_count == 1)
        {
            m_min = dt;
            m_max = dt;
            return;
        }

        // If the evicted sample held the current extreme, the true new extreme could
        // be anywhere in the window — fall back to a full rescan. Otherwise the
        // incoming sample can only extend (never narrow) the existing min/max, O(1).
        if (wasFull && (evicted == oldMin || evicted == oldMax))
        {
            RescanMinMax();
        }
        else
        {
            if (dt < m_min) m_min = dt;
            if (dt > m_max) m_max = dt;
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
            if (v < minDt) minDt = v;
            if (v > maxDt) maxDt = v;
        }
        m_min = minDt;
        m_max = maxDt;
    }
}
