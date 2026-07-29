/**
 * @file Stages.hpp
 * @author sumin.park
 * @brief Anchor stage tag types; identity is TypeIdOf<Tag>().
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once

namespace engine::stages
{
    struct Input
    {
    };
    struct FixedMain
    {
    }; // driver only, holds no systems; see Schedule::RunFixedMain
    struct FixedUpdate
    {
    }; // contained in FixedMain
    struct Update
    {
    };
    struct PostUpdate
    {
    };
    struct Last
    {
    };
}
