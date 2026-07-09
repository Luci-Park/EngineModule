#pragma once
#include <engine/core/Application.hpp>

// Sandbox application. No window/input yet (headless this unit) — exercises the
// core loop, then closes itself after a fixed run so it terminates cleanly.
// The FPS-lock keybind demo lands once the window/overlay chunk exists.
class SandboxApp final : public engine::Application
{
public:
    void OnUpdate(float dt) override;

private:
    static constexpr float RUN_SECONDS = 3.0f;
    float m_elapsed = 0.0f;
};
