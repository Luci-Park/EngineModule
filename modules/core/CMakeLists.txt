engine_add_module(core
    src/Core.cpp
    src/Application.cpp
    src/Clock.cpp
    src/FrameTimer.cpp
    src/NullPacer.cpp
    src/FixedRatePacer.cpp
    src/Run.cpp
    src/log/Log.cpp
)
engine_apply_sanitizers(engine_core)

# private
find_package(spdlog CONFIG REQUIRED)
target_link_libraries(engine_core PRIVATE spdlog::spdlog)
