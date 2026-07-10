#pragma once
#include <engine/core/Application.hpp>

class SandboxApp final : public engine::Application
{
public:
    void OnInit() override;
    void OnUpdate(float dt) override;
    void OnShutdown() override;

private:
    static constexpr float RUN_SECONDS = 3.0f;
    float m_elapsed = 0.0f;
};
