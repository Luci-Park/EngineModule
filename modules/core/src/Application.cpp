#include <engine/core/Application.hpp>
#include <engine/core/NullPacer.hpp>

namespace engine
{
    Application::~Application() = default;

    IFramePacer& Application::Pacer() const
    {
        // Lazily-constructed shared default; avoids static-init-order issues and
        // means an app that never sets a pacer still gets uncapped behavior.
        static NullPacer s_default;
        return m_pacer ? *m_pacer : s_default;
    }
}
