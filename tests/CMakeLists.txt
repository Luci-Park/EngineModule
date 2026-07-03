find_package(Catch2 3 REQUIRED)

add_executable(core_tests core/CoreTest.cpp)

target_link_libraries(core_tests PRIVATE engine::core Catch2::Catch2WithMain)
engine_set_warnings(core_tests)

include(CTest)
include(Catch)
catch_discover_tests(core_tests)