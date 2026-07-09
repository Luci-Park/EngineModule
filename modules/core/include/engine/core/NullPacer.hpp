#pragma once
#include <engine/core/IFramePacer.hpp>

namespace engine
{
    // Default pacer: no cap, no stall. The loop runs as fast as it can.
    class ENGINE_CORE_API NullPacer final : public IFramePacer
    {
    public:
        void EndFrame(const FrameStats& stats) override;
    };
}
