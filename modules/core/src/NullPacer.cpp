/**
 * @file NullPacer.cpp
 * @author sumin.park
 * @brief Uncapped pacer implementation.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#include <engine/core/NullPacer.hpp>

namespace engine
{
    void NullPacer::EndFrame(const FrameStats & /*stats*/) // unnamed -> no unused-param warning under /WX
    {
    }
}
