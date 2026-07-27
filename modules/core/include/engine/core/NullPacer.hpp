/**
 * @file NullPacer.hpp
 * @author sumin.park
 * @brief Default frame pacer that never stalls, leaving the loop uncapped.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include <engine/core/IFramePacer.hpp>

namespace engine
{
    // default pacer; never stalls, so the loop runs uncapped
    class ENGINE_CORE_API NullPacer final : public IFramePacer
    {
    public:
        void EndFrame(const FrameStats &stats) override;
    };
}
