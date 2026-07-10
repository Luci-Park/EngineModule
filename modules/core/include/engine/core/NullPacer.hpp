#pragma once
#include <engine/core/IFramePacer.hpp>

namespace engine
{
    // default pacer, unfixed
    class ENGINE_CORE_API NullPacer final : public IFramePacer
    {
    public:
        void EndFrame(const FrameStats &stats) override;
    };
}
