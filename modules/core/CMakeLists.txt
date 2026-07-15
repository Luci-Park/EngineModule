engine_add_module(core
    src/Core.cpp
    src/Application.cpp
    src/Clock.cpp
    src/FrameTimer.cpp
    src/NullPacer.cpp
    src/FixedRatePacer.cpp
    src/Run.cpp
    src/log/Log.cpp
    src/memory/Memory.cpp
    src/memory/LinearAllocator.cpp
    src/memory/PoolAllocator.cpp
    src/profile/Profile.cpp
    src/ecs/EntityAllocator.cpp
    src/ecs/World.cpp
)
engine_apply_sanitizers(engine_core)
if(MSVC)
    # EntityAllocator's private STL members trip C4251 under ENGINE_SHARED; spdlog's interface
    # flags happen to suppress it today -- make the suppression explicit (see ENGINE_RULES.md)
    target_compile_options(engine_core PRIVATE /wd4251)
endif()

# private
find_package(spdlog CONFIG REQUIRED)
target_link_libraries(engine_core PRIVATE spdlog::spdlog)

# PUBLIC -> the profiler macros compile to real calls in consumers (sandbox) too.
if(ENGINE_PROFILE)
    target_compile_definitions(engine_core PUBLIC ENGINE_PROFILE_ENABLED=1)
endif()
