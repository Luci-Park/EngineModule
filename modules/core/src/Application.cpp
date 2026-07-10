#include <engine/core/Application.hpp>
#include <engine/core/NullPacer.hpp>

namespace engine
{
    Application::~Application() = default;

    IFramePacer &Application::Pacer() const
    {
        static NullPacer s_default;
        return m_pacer ? *m_pacer : s_default;
    }
}
