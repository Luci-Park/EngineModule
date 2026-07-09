#pragma once
#include <engine/core/core_export.h>
#include <cstdint>

namespace engine
{
    // Cheap, copyable per-frame snapshot. Feeds pacers and the debug overlay.
    // History (the ring buffer) is NOT copied in here — it lives on FrameTimer
    // (its owner); the overlay reads it via FrameTimer::History() to avoid copying
    // the whole ring every frame.
    struct FrameStats
    {
        float dt = 0.0f;      // last frame delta, seconds
        float fps = 0.0f;     // instantaneous, 1/dt
        float avgFps = 0.0f;  // rolling average over the ring window
        float minDt = 0.0f;   // min frame time over the ring window, seconds
        float maxDt = 0.0f;   // max frame time over the ring window, seconds
    };

    // Per-frame delta time via a monotonic clock, plus a ring buffer of recent
    // frame times (>= 2 seconds at typical frame rates) for charting.
    class ENGINE_CORE_API FrameTimer
    {
    public:
        // Ring capacity: covers >= 2s up to ~512 fps.
        static constexpr uint32_t HISTORY_CAPACITY = 1024;

        FrameTimer();

        // Advance one frame. Returns the delta since the previous Tick, in seconds.
        // First call returns 0 (no prior frame).
        float Tick();

        const FrameStats& Stats() const { return m_stats; }

        // Frame-time ring (seconds). Oldest sample first when full; [0, HistoryCount)
        // when still filling. For the overlay's frame-time chart.
        const float* History() const { return m_history; }
        uint32_t HistoryCount() const { return m_count < HISTORY_CAPACITY ? m_count : HISTORY_CAPACITY; }
        uint32_t HistoryCapacity() const { return HISTORY_CAPACITY; }

    private:
        void Push(float dt);
        void RescanMinMax();  // O(history) fallback; only when eviction removes the current extreme

        int64_t m_lastNs = 0;
        bool m_started = false;

        float m_history[HISTORY_CAPACITY] = {};
        uint32_t m_head = 0;   // next write index
        uint32_t m_count = 0;  // samples in the window, saturates at HISTORY_CAPACITY (never wraps)

        // Running aggregates, updated incrementally in Push() so Tick() stays O(1)
        // amortized instead of rescanning the whole ring every frame.
        float m_sum = 0.0f;
        float m_min = 0.0f;
        float m_max = 0.0f;

        FrameStats m_stats{};
    };
}
