#include <engine/core/NullPacer.hpp>

namespace engine
{
    // Out-of-line definition anchors the vtable in this TU and keeps the exported
    // symbol pinned across the shared-library boundary.
    void NullPacer::EndFrame(const FrameStats& /*stats*/)
    {
        // no-op: uncapped
    }
}
