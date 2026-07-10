#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <engine/core/profile/Profile.hpp>

using namespace engine;
using engine::detail::ProfileFrameMark;
using engine::detail::ProfilePushEvent;

namespace
{
    // stable literal-pointer keys (aggregation keys on the pointer, not the text).
    constexpr const char* ZONE_A = "A";
    constexpr const char* ZONE_B = "B";

    const ProfileZone* Find(const ProfileSnapshot& s, const char* name)
    {
        for (uint32_t i = 0; i < s.count; ++i)
            if (s.zones[i].name == name) return &s.zones[i];
        return nullptr;
    }
}

TEST_CASE("frame mark aggregates events per zone", "[core][profile]")
{
    ProfileFrameMark();  // clear anything from earlier in this process

    ProfilePushEvent(ZONE_A, 0, 1'000'000);   // 1.0 ms
    ProfilePushEvent(ZONE_A, 0, 2'000'000);   // 2.0 ms
    ProfilePushEvent(ZONE_B, 0, 500'000);     // 0.5 ms
    ProfileFrameMark();

    const auto snap = ProfileLastFrame();

    const ProfileZone* a = Find(snap, ZONE_A);
    const ProfileZone* b = Find(snap, ZONE_B);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(a->callCount == 2);
    REQUIRE(a->totalMs == Catch::Approx(3.0));
    REQUIRE(b->callCount == 1);
    REQUIRE(b->totalMs == Catch::Approx(0.5));
}

TEST_CASE("zones are sorted by total time descending", "[core][profile]")
{
    ProfileFrameMark();

    ProfilePushEvent(ZONE_B, 0, 500'000);     // 0.5 ms
    ProfilePushEvent(ZONE_A, 0, 5'000'000);   // 5.0 ms
    ProfileFrameMark();

    const auto snap = ProfileLastFrame();
    REQUIRE(snap.count >= 2);
    REQUIRE(snap.zones[0].totalMs >= snap.zones[1].totalMs);
    REQUIRE(snap.zones[0].name == ZONE_A);  // largest first
}

TEST_CASE("frame mark clears events for the next frame", "[core][profile]")
{
    ProfileFrameMark();
    ProfilePushEvent(ZONE_A, 0, 1'000'000);
    ProfileFrameMark();

    ProfileFrameMark();  // second mark with no new events
    const auto snap = ProfileLastFrame();
    REQUIRE(Find(snap, ZONE_A) == nullptr);
}
