find_package(Catch2 3 REQUIRED)

add_executable(core_tests
    core/CoreTest.cpp
    core/FrameTimerTest.cpp
    core/LogTest.cpp
    core/AllocatorTest.cpp
    core/ProfileTest.cpp
)

target_link_libraries(core_tests PRIVATE engine::core Catch2::Catch2WithMain)
engine_set_warnings(core_tests)
# must match engine_core sanitizer state or ASan annotations mismatch at link (LNK2038)
engine_apply_sanitizers(core_tests)
engine_target_pch(core_tests)

include(CTest)
include(Catch)
catch_discover_tests(core_tests)