#include "SandboxApp.hpp"

void SandboxApp::OnUpdate(float dt)
{
    m_elapsed += dt;
    if (m_elapsed >= RUN_SECONDS)
    {
        Close();
    }
}
