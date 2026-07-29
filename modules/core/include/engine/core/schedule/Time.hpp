/**
 * @file Time.hpp
 * @author sumin.park
 * @brief Time contexts: variable frame time and fixed-step sim time, never mixed.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include <cstdint>

namespace engine
{
    // variable frame time; written by Schedule::RunFrame once per frame
    struct Time
    {
        float m_delta = 0.0f;
        double m_elapsed = 0.0;
        uint64_t m_frameCount = 0;
    };

    // fixed step, inside FixedMain only; written by the FixedMain driver per sub-step
    struct FixedTime
    {
        float m_delta = 0.0f;     // always == the step
        double m_elapsed = 0.0;
        uint32_t m_stepIndex = 0; // which sub-step within this frame
    };
}
