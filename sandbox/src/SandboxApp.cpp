#include "SandboxApp.hpp"

#include <engine/core/log/Log.hpp>
#include <engine/core/memory/Memory.hpp>
#include <engine/core/profile/Profile.hpp>

void SandboxApp::OnInit()
{
    // might move this to run if all plugins need it
    engine::InitLog();
    ENGINE_LOG_INFO("SandboxApp init — running headless for {}s", RUN_SECONDS);
}

void SandboxApp::OnUpdate(float dt)
{
    ENGINE_PROFILE_FUNCTION();
    m_elapsed += dt;
    if (m_elapsed >= RUN_SECONDS)
    {
        Close();
    }
}

void SandboxApp::OnShutdown()
{
    ENGINE_LOG_INFO("SandboxApp shutdown after {:.2f}s", m_elapsed);

    const auto stats = engine::Stats();
    ENGINE_LOG_INFO("memory: {} bytes current, {} bytes peak, {} live allocs",
                    stats.currentBytes, stats.peakBytes, stats.allocationCount);
    engine::ReportLeaks();

    const auto prof = engine::ProfileLastFrame();
    for (uint32_t i = 0; i < prof.count; ++i)
    {
        ENGINE_LOG_INFO("profile: {} {:.3f}ms x{}", prof.zones[i].name,
                        prof.zones[i].totalMs, prof.zones[i].callCount);
    }

    engine::FlushLog();
}
