#include <engine/core/memory/Memory.hpp>
#include <engine/core/log/Log.hpp>
#include <engine/core/log/Assert.hpp>
#include "Align.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <mutex>
#include <new>

#ifndef NDEBUG
#include <unordered_map>
#endif

// header-prefixed allocation: [padding][AllocHeader][user ptr...]. header carries
// size+align+tag so DeallocateRaw needs nothing but the returned ptr -> lightweight
// stats stay correct in every build, not just Debug.

namespace engine
{
    namespace
    {
        constexpr std::size_t TAG_COUNT = static_cast<std::size_t>(MemoryTag::Count);

        std::atomic<std::size_t> g_currentBytes{0};
        std::atomic<std::size_t> g_peakBytes{0};
        std::atomic<std::size_t> g_allocCount{0};
        std::atomic<std::size_t> g_perTagBytes[TAG_COUNT] = {};

        struct AllocHeader
        {
            std::size_t size;
            std::size_t align; // effective align actually passed to operator new
            MemoryTag tag;
        };

        void RecordAlloc(std::size_t size, MemoryTag tag)
        {
            const std::size_t cur = g_currentBytes.fetch_add(size, std::memory_order_relaxed) + size;
            std::size_t peak = g_peakBytes.load(std::memory_order_relaxed);
            while (cur > peak && !g_peakBytes.compare_exchange_weak(peak, cur, std::memory_order_relaxed))
            {
            }
            g_allocCount.fetch_add(1, std::memory_order_relaxed);
            g_perTagBytes[static_cast<std::size_t>(tag)].fetch_add(size, std::memory_order_relaxed);
        }

        void RecordFree(std::size_t size, MemoryTag tag)
        {
            g_currentBytes.fetch_sub(size, std::memory_order_relaxed);
            g_allocCount.fetch_sub(1, std::memory_order_relaxed);
            g_perTagBytes[static_cast<std::size_t>(tag)].fetch_sub(size, std::memory_order_relaxed);
        }

#ifndef NDEBUG
        struct LiveAlloc
        {
            std::size_t size;
            MemoryTag tag;
            const char *file;
            int line;
        };

        struct Tracker
        {
            std::mutex mutex;
            std::unordered_map<void *, LiveAlloc> live;
        };

        // leaky, never destroyed -> no static-dtor-order hazard (bit us in logging)
        Tracker &GetTracker()
        {
            static Tracker *t = new Tracker();
            return *t;
        }
#endif
    }

    void *AllocateRaw(std::size_t size, std::size_t align, AllocSite site)
    {
        // header sits just before userPtr -> must itself be aligned for AllocHeader.
        const std::size_t effAlign = std::max(align, alignof(AllocHeader));
        const std::size_t headerSpace = AlignUp(sizeof(AllocHeader), effAlign);
        void *raw = ::operator new(headerSpace + size, std::align_val_t(effAlign), std::nothrow);
        if (!raw)
            return nullptr;

        void *userPtr = static_cast<std::byte *>(raw) + headerSpace;
        auto *header = reinterpret_cast<AllocHeader *>(userPtr) - 1;
        header->size = size;
        header->align = effAlign; // store effective -> DeallocateRaw stays consistent
        header->tag = site.tag;

        RecordAlloc(size, site.tag);

#ifndef NDEBUG
        {
            Tracker &t = GetTracker();
            std::lock_guard<std::mutex> lock(t.mutex);
            t.live[userPtr] = LiveAlloc{size, site.tag, site.loc.file_name(), static_cast<int>(site.loc.line())};
        }
#endif
        return userPtr;
    }

    void DeallocateRaw(void *ptr)
    {
        if (!ptr)
            return;

        const auto *header = reinterpret_cast<AllocHeader *>(ptr) - 1;
        const std::size_t size = header->size;
        const std::size_t align = header->align;
        const MemoryTag tag = header->tag;

        RecordFree(size, tag);

#ifndef NDEBUG
        {
            Tracker &t = GetTracker();
            std::lock_guard<std::mutex> lock(t.mutex);
            t.live.erase(ptr);
        }
#endif

        void *raw = static_cast<std::byte *>(ptr) - AlignUp(sizeof(AllocHeader), align);
        ::operator delete(raw, std::align_val_t(align));
    }

    MemoryStats Stats()
    {
        MemoryStats s;
        s.currentBytes = g_currentBytes.load(std::memory_order_relaxed);
        s.peakBytes = g_peakBytes.load(std::memory_order_relaxed);
        s.allocationCount = g_allocCount.load(std::memory_order_relaxed);
        for (std::size_t i = 0; i < TAG_COUNT; ++i)
        {
            s.perTagBytes[i] = g_perTagBytes[i].load(std::memory_order_relaxed);
        }
        return s;
    }

    std::size_t LiveAllocationCount()
    {
#ifndef NDEBUG
        Tracker &t = GetTracker();
        std::lock_guard<std::mutex> lock(t.mutex);
        return t.live.size();
#else
        return 0; // no per-alloc data in Release
#endif
    }

    void ReportLeaks()
    {
#ifndef NDEBUG
        Tracker &t = GetTracker();
        std::lock_guard<std::mutex> lock(t.mutex);

        for (const auto &[ptr, alloc] : t.live)
        {
            ENGINE_LOG_ERROR("leak: {} bytes, tag={}, {}:{}", alloc.size,
                             static_cast<int>(alloc.tag), alloc.file, alloc.line);
        }
        ENGINE_ASSERT(t.live.empty(), "{} live allocation(s) at shutdown", t.live.size());
#endif
    }
}
