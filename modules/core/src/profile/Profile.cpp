#include <engine/core/profile/Profile.hpp>
#include "../Clock.hpp"

#include <algorithm>
#include <mutex>
#include <new>
#include <vector>

namespace engine
{
    namespace
    {
        constexpr uint32_t BUFFER_CAP = 8192;

        struct Event
        {
            const char *name;
            int64_t startNs;
            int64_t endNs;
        };

        struct ThreadBuffer
        {
            Event events[BUFFER_CAP];
            uint32_t count = 0;
            uint32_t dropped = 0;
        };

        struct ProfileState
        {
            std::mutex mutex;
            std::vector<ThreadBuffer *> buffers;
            ProfileSnapshot lastFrame;
        };

        ProfileState &State()
        {
            static ProfileState *s = new ProfileState();
            return *s;
        }

        thread_local ThreadBuffer *t_buffer = nullptr;

        ThreadBuffer *LocalBuffer()
        {
            if (!t_buffer)
            {
                auto *buf = new (std::nothrow) ThreadBuffer();
                if (!buf)
                    return nullptr;

                ProfileState &st = State();
                std::lock_guard<std::mutex> lock(st.mutex);
                try
                {
                    st.buffers.push_back(buf);
                }
                catch (...)
                {
                    delete buf;
                    return nullptr;
                }
                t_buffer = buf;
            }
            return t_buffer;
        }

        void Accumulate(ProfileSnapshot &snap, const char *name, int64_t durNs)
        {
            const double ms = static_cast<double>(durNs) * 1e-6;
            for (uint32_t i = 0; i < snap.count; ++i)
            {
                if (snap.zones[i].name == name) // literal-pointer identity
                {
                    snap.zones[i].totalMs += ms;
                    ++snap.zones[i].callCount;
                    return;
                }
            }
            if (snap.count < ProfileSnapshot::MAX_ZONES)
            {
                snap.zones[snap.count] = ProfileZone{name, ms, 1};
                ++snap.count;
            }
            else
            {
                ++snap.droppedEvents; // zone table full -> surface the loss
            }
        }
    }

    namespace detail
    {
        int64_t ProfileClockNs()
        {
            return NowNs();
        }

        void ProfilePushEvent(const char *name, int64_t startNs, int64_t endNs)
        {
            ThreadBuffer *buf = LocalBuffer();
            if (!buf)
                return; // OOM at first touch -> drop
            if (buf->count < BUFFER_CAP)
            {
                buf->events[buf->count++] = Event{name, startNs, endNs};
            }
            else
            {
                ++buf->dropped;
            }
        }

        void ProfileFrameMark()
        {
            ProfileState &st = State();
            ProfileSnapshot snap;
            uint32_t dropped = 0;

            std::lock_guard<std::mutex> lock(st.mutex);
            for (ThreadBuffer *buf : st.buffers)
            {
                for (uint32_t i = 0; i < buf->count; ++i)
                {
                    const Event &e = buf->events[i];
                    Accumulate(snap, e.name, e.endNs - e.startNs);
                }
                dropped += buf->dropped;
                buf->count = 0;
                buf->dropped = 0;
            }
            snap.droppedEvents += dropped;

            std::sort(snap.zones, snap.zones + snap.count,
                      [](const ProfileZone &a, const ProfileZone &b)
                      { return a.totalMs > b.totalMs; });

            st.lastFrame = snap;
        }
    }

    ProfileSnapshot ProfileLastFrame()
    {
        ProfileState &st = State();
        std::lock_guard<std::mutex> lock(st.mutex);
        return st.lastFrame;
    }
}
