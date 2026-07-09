find_package(Catch2 3 REQUIRED)

add_executable(core_tests
    core/CoreTest.cpp
    core/FrameTimerTest.cpp
)

target_link_libraries(core_tests PRIVATE engine::core Catch2::Catch2WithMain)
engine_set_warnings(core_tests)
# Must match engine_core's sanitizer state or ASan container annotations mismatch at link
# (LNK2038 annotate_vector/optional). Explicit per-target opt-in, per ENGINE_RULES.md.
engine_apply_sanitizers(core_tests)
engine_target_pch(core_tests)

include(CTest)
include(Catch)
catch_discover_tests(core_tests)