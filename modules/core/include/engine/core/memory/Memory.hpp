#pragma once
#include <engine/core/core_export.h>

#include <cstddef>
#include <cstdint>
#include <source_location>
#include <utility>

namespace engine
{
    enum class MemoryTag : uint8_t
    {
        General = 0,
        Entities,
        Components,
        Events,
        Scratch,
        Assets,
        Count // array size
    };

    // adding a MemoryTag changes sizeof(MemoryStats) -> shared-lib API break
    struct MemoryStats
    {
        std::size_t currentBytes = 0;
        std::size_t peakBytes = 0;
        std::size_t allocationCount = 0;
        std::size_t perTagBytes[static_cast<std::size_t>(MemoryTag::Count)] = {};
    };

    // position + type
    struct AllocSite
    {
        MemoryTag tag;
        std::source_location loc;

        constexpr AllocSite(MemoryTag t, std::source_location l = std::source_location::current())
            : tag(t), loc(l)
        {
        }
    };

    // raw tracked alloc, one entry per call -> for arena owners (Linear/Pool) only
    ENGINE_CORE_API void *AllocateRaw(std::size_t size, std::size_t align, AllocSite site);
    ENGINE_CORE_API void DeallocateRaw(void *ptr);

    ENGINE_CORE_API MemoryStats Stats();

    // non-fatal query. Debug: live tracked count / Release: always 0
    ENGINE_CORE_API std::size_t LiveAllocationCount();

    // policy: Debug logs each live alloc + ENGINE_ASSERTs on any leak. Release: no-op.
    ENGINE_CORE_API void ReportLeaks();

    template <typename T, typename... Args>
    T *Allocate(AllocSite site, Args &&...args)
    {
        void *mem = AllocateRaw(sizeof(T), alignof(T), site);
        if (!mem)
            return nullptr;
        return ::new (mem) T(std::forward<Args>(args)...);
    }

    // T must be the concrete type Allocate<T> returned -> not a base subobject ptr
    // (interior-ptr/non-virtual-dtor is UB, same as raw delete-through-base)
    template <typename T>
    void Delete(T *ptr)
    {
        if (!ptr)
            return;
        ptr->~T();
        DeallocateRaw(ptr);
    }
}
