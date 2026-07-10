#pragma once
#include <engine/core/core_export.h>

#include <cstdint>

namespace engine
{
    // one zone's inclusive total over the last completed frame.
    struct ProfileZone
    {
        const char *name = nullptr; // literal pointer, aggregation key
        double totalMs = 0.0;       // inclusive of nested scopes
        uint32_t callCount = 0;
    };

    struct ProfileSnapshot
    {
        static constexpr uint32_t MAX_ZONES = 64;
        ProfileZone zones[MAX_ZONES]; // sorted by totalMs desc, [0, count)
        uint32_t count = 0;
        uint32_t droppedEvents = 0; // events lost to buffer overflow last frame
    };

    ENGINE_CORE_API ProfileSnapshot ProfileLastFrame();

    namespace detail
    {
        ENGINE_CORE_API int64_t ProfileClockNs();
        ENGINE_CORE_API void ProfilePushEvent(const char *name, int64_t startNs, int64_t endNs);
        ENGINE_CORE_API void ProfileFrameMark();

        class ProfileScope
        {
        public:
            explicit ProfileScope(const char *name) : m_name(name), m_start(ProfileClockNs()) {}
            ~ProfileScope() { ProfilePushEvent(m_name, m_start, ProfileClockNs()); }

            ProfileScope(const ProfileScope &) = delete;
            ProfileScope &operator=(const ProfileScope &) = delete;

        private:
            const char *m_name;
            int64_t m_start;
        };
    }
}

#define ENGINE_PROFILE_CONCAT2(a, b) a##b
#define ENGINE_PROFILE_CONCAT(a, b) ENGINE_PROFILE_CONCAT2(a, b)

#ifdef ENGINE_PROFILE_ENABLED
#define ENGINE_PROFILE_SCOPE(name) \
    ::engine::detail::ProfileScope ENGINE_PROFILE_CONCAT(_engineProfileScope_, __COUNTER__)(name)
#define ENGINE_PROFILE_FUNCTION() ENGINE_PROFILE_SCOPE(__func__)
#define ENGINE_PROFILE_FRAME() ::engine::detail::ProfileFrameMark()
#else
#define ENGINE_PROFILE_SCOPE(name) ((void)0)
#define ENGINE_PROFILE_FUNCTION() ((void)0)
#define ENGINE_PROFILE_FRAME() ((void)0)
#endif
