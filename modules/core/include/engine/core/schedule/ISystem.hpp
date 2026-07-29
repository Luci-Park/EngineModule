/**
 * @file ISystem.hpp
 * @author sumin.park
 * @brief System interface: Update every run, optional OnStart/OnStop lifecycle.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include <engine/core/core_export.h>

#include <cstdint>

namespace engine
{
    class World;

    class ENGINE_CORE_API ISystem
    {
    public:
        virtual ~ISystem() = default;

        virtual void Update(World &world) = 0;
        virtual void OnStart(World &world) { (void)world; }
        virtual void OnStop(World &world) { (void)world; }

        // scheduler-written; a system reads it to build Changed<T> / Added<T> query filters
        uint32_t LastRunTick() const { return m_lastRunTick; }

    private:
        friend class Schedule; // stamps m_lastRunTick AFTER Update, never before

        uint32_t m_lastRunTick = 0;
    };
}
