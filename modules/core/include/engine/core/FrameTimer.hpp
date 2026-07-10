#pragma once
#include <engine/core/core_export.h>
#include <cstdint>

namespace engine
{
    struct FrameStats
    {
        float dt = 0.0f;     // last delta, s
        float fps = 0.0f;    // 1/dt
        float avgFps = 0.0f; // rolling avg over window
        float minDt = 0.0f;  // min over window, s
        float maxDt = 0.0f;  // max over window, s
    };

    class ENGINE_CORE_API FrameTimer
    {
    public:
        // for profiling and such
        static constexpr uint32_t HISTORY_CAPACITY = 1024; // >= 2s up to ~512 fps

        FrameTimer();

        float Tick();

        const FrameStats &Stats() const { return m_stats; }

        // for profiling and such (s)
        const float *History() const { return m_history; }
        uint32_t HistoryCount() const { return m_count < HISTORY_CAPACITY ? m_count : HISTORY_CAPACITY; }
        uint32_t HistoryCapacity() const { return HISTORY_CAPACITY; }

    private:
        // push history
        void Push(float dt);
        void RescanMinMax(); // execute when current min/max pass save history

        int64_t m_lastNs = 0;
        bool m_started = false;

        float m_history[HISTORY_CAPACITY] = {};
        uint32_t m_head = 0;  // next write
        uint32_t m_count = 0; // samples, saturates at capacity

        double m_sum = 0.0;
        float m_min = 0.0f;
        float m_max = 0.0f;

        FrameStats m_stats{};
    };
}
